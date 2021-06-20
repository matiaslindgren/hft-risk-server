#ifndef INCLUDED_RISKSERVICE_SERVER_HEADER
#define INCLUDED_RISKSERVICE_SERVER_HEADER
/*
 * Main service that implements the risk server.
 */

#include "format.h"
#include "tcp.h"
#include <string>
#include <unordered_map>
#include <utility>

namespace rs {

struct InstrumentState {
  using NetPos = long long;
  NetPos net_pos{0};

  Quantity buy_qty{0};
  Quantity sell_qty{0};

  NetPos worst_buy_pos() const noexcept {
    auto qty = static_cast<NetPos>(buy_qty);
    return std::max(qty, net_pos + qty);
  }

  NetPos worst_sell_pos() const noexcept {
    auto qty = static_cast<NetPos>(sell_qty);
    return std::max(qty, qty - net_pos);
  }
};

struct Order {
  ListingID listing_id;
  Quantity quantity;
  char side;
};

class RiskService {

public:
  explicit RiskService(const std::string &address, const std::string &tcp_port,
                       Quantity max_buy, Quantity max_sell)
      : tcp_server_(address, tcp_port), online_(false), max_buy_pos_(max_buy),
        max_sell_pos_(max_sell) {}

  // Rule of five with same constraints as in tcp::Server (no copy but move ok).
  ~RiskService() noexcept = default;

  // Disallow copying, we don't want two servers handling requests at the same
  // port.
  RiskService(const RiskService &) = delete;
  RiskService &operator=(const RiskService &) = delete;

  // Allow memberwise moves.
  RiskService(RiskService &&other) noexcept = default;
  RiskService &operator=(RiskService &&other) noexcept = default;

  // Wait for incoming requests and handle them.
  void wait();

  void stop() noexcept { online_ = false; }

  // Dump full state of server.
  std::string dump_state() const {
    std::string s = "\n";
    s += rs::format("max buy position: {}\nmax sell position: {}\n",
                    max_buy_pos_, max_sell_pos_);
    s += "orders: \n";
    for (const auto &[id, order] : std::as_const(orders_)) {
      s += rs::format("  id: {}\n", id);
      s += rs::format("    listing_id: {}\n", order.listing_id);
      s += rs::format("    quantity: {}\n", order.quantity);
      s += rs::format("    side: {}\n", std::string{order.side});
    }
    s += "instrument state: \n";
    for (const auto &[id, state] : std::as_const(instrument_state_)) {
      s += rs::format("  id: {}\n", id);
      s += rs::format("    net_pos: {}\n", state.net_pos);
      s += rs::format("    buy_qty: {}\n", state.buy_qty);
      s += rs::format("    sell_qty: {}\n", state.sell_qty);
      s += rs::format("    worst_buy_pos: {}\n", state.worst_buy_pos());
      s += rs::format("    worst_sell_pos: {}\n", state.worst_sell_pos());
    }
    return s;
  }

private:
  tcp::Server tcp_server_;
  bool online_;

  Quantity max_buy_pos_;
  Quantity max_sell_pos_;

  std::unordered_map<OrderID, Order> orders_;
  std::unordered_map<ListingID, InstrumentState> instrument_state_;

  // Read messages from client until they close the connection.
  bool serve_client(const tcp::Socket &);

  // Message handlers.

  [[nodiscard]] protocol::OrderResponse
  handle_new_order(const protocol::NewOrder &);
  [[nodiscard]] protocol::OrderResponse
  handle_modify_order(const protocol::ModifyOrderQuantity &);
  void handle_delete_order(const protocol::DeleteOrder &);
  void handle_trade(const protocol::Trade &);

  // Helpers for message handlers.

  bool register_new_order(OrderID, const Order &);
  bool update_order_quantity(Order &, Quantity);
  void delete_order(OrderID);
};

} // namespace rs

#endif // INCLUDED_RISKSERVICE_SERVER_HEADER
