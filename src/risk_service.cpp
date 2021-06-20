#include "risk_service.h"
#include "logging.h"

namespace rs {

auto logger = logging::make_logger("risk_service", logging::Level::DEBUG);

void RiskService::wait() {
  logger->info("Waiting for connections");
  online_ = true;
  while (online_) {
    try {
      auto socket = tcp_server_.next_connection();
      logger->debug("New connection on socket {}, reading all messages",
                    socket.fd);
      while (serve_client(socket))
        ;
    } catch (const std::exception &error) {
      logger->error(error.what());
    }
    logger->info(dump_state());
  }
}

bool RiskService::serve_client(const tcp::Socket &socket) {
  using namespace protocol;

  auto msg = tcp_server_.receive_message(socket);
  if (msg.empty()) {
    // Client closed connection.
    return false;
  }

  auto header = decode_header(msg);
  logger->info("Handling message of type {}", header.version);

  switch (header.version) {
  case NewOrder::MESSAGE_TYPE: {
    auto response = handle_new_order(decode_payload<NewOrder>(msg));
    Header header{OrderResponse::MESSAGE_TYPE, sizeof(response), 1, now()};
    tcp_server_.send_message(socket, encode(header, response));
  } break;

  case DeleteOrder::MESSAGE_TYPE: {
    handle_delete_order(decode_payload<DeleteOrder>(msg));
  } break;

  case ModifyOrderQuantity::MESSAGE_TYPE: {
    auto response =
        handle_modify_order(decode_payload<ModifyOrderQuantity>(msg));
    Header header{OrderResponse::MESSAGE_TYPE, sizeof(response), 1, now()};
    tcp_server_.send_message(socket, encode(header, response));
  } break;

  case Trade::MESSAGE_TYPE: {
    handle_trade(decode_payload<Trade>(msg));
  } break;

  default: {
    logger->warn("Ignoring unknown protocol version {}", header.version);
  }
  }

  return true;
}

[[nodiscard]] protocol::OrderResponse
RiskService::handle_new_order(const protocol::NewOrder &create_msg) {
  using protocol::OrderResponse;
  logger->debug("Handling creation of order {}", create_msg.orderId);

  OrderResponse response{OrderResponse::MESSAGE_TYPE, create_msg.orderId,
                         // Reject by default and change to accept only if order
                         // creation succeeded.
                         OrderResponse::Status::REJECTED};

  if (create_msg.side != 'B' && create_msg.side != 'S') {
    logger->warn("Ignoring new order with unknown side {}", create_msg.side);
    return response;
  }

  // Try inserting a new order, if it is valid.
  Order order{create_msg.listingId, create_msg.orderQuantity, create_msg.side};
  if (register_new_order(create_msg.orderId, std::move(order))) {
    response.status = OrderResponse::Status::ACCEPTED;
  }

  return response;
}

[[nodiscard]] protocol::OrderResponse RiskService::handle_modify_order(
    const protocol::ModifyOrderQuantity &modify_msg) {
  using protocol::OrderResponse;
  logger->debug("Handling modification of order {}", modify_msg.orderId);

  OrderResponse response{OrderResponse::MESSAGE_TYPE, modify_msg.orderId,
                         OrderResponse::Status::REJECTED};

  auto id = modify_msg.orderId;
  auto order_it = orders_.find(id);
  if (order_it == orders_.end()) {
    // Cannot modify non-existing order.
    return response;
  }

  if (update_order_quantity(order_it->second, modify_msg.newQuantity)) {
    // Modification succeeded.
    response.status = OrderResponse::Status::ACCEPTED;
  }

  return response;
}

void RiskService::handle_delete_order(const protocol::DeleteOrder &delete_msg) {
  logger->debug("Handling deletion of order {}", delete_msg.orderId);
  delete_order(delete_msg.orderId);
}

void RiskService::handle_trade(const protocol::Trade &trade_msg) {
  logger->debug("Handling trade {} of listing {}", trade_msg.tradeId,
                trade_msg.listingId);
  auto &state = instrument_state_[trade_msg.listingId];
  const auto &order = orders_[trade_msg.tradeId];
  switch (order.side) {
  case 'B': {
    state.net_pos -= trade_msg.tradeQuantity;
  } break;
  case 'S': {
    state.net_pos += trade_msg.tradeQuantity;
  } break;
  }
}

bool RiskService::register_new_order(OrderID id, const Order &order) {
  bool accepted = false;
  auto &state = instrument_state_[order.listing_id];

  switch (order.side) {
  case 'B': {
    if (order.quantity + state.worst_buy_pos() <= max_buy_pos_) {
      orders_[id] = order;
      state.buy_qty += order.quantity;
      accepted = true;
    }
  } break;
  case 'S': {
    if (order.quantity + state.worst_sell_pos() <= max_sell_pos_) {
      orders_[id] = order;
      state.sell_qty += order.quantity;
      accepted = true;
    }
  } break;
  }

  return accepted;
}

bool RiskService::update_order_quantity(Order &order, Quantity new_qty) {
  bool accepted = false;
  auto old_qty = order.quantity;
  auto &state = instrument_state_[order.listing_id];

  switch (order.side) {
  case 'B': {
    if (new_qty - old_qty + state.worst_buy_pos() <= max_buy_pos_) {
      state.buy_qty += new_qty - old_qty;
      order.quantity = new_qty;
      accepted = true;
    }
  } break;
  case 'S': {
    if (new_qty - old_qty + state.worst_sell_pos() <= max_sell_pos_) {
      state.sell_qty += new_qty - old_qty;
      order.quantity = new_qty;
      accepted = true;
    }
  } break;
  }

  return accepted;
}

void RiskService::delete_order(OrderID id) {
  auto order_it = orders_.find(id);
  if (order_it == orders_.end()) {
    return;
  }
  const auto &order = order_it->second;
  auto &state = instrument_state_[order.listing_id];
  switch (order.side) {
  case 'B': {
    state.buy_qty -= order.quantity;
  } break;
  case 'S': {
    state.sell_qty -= order.quantity;
  } break;
  }
  orders_.erase(order_it);
}

} // namespace rs
