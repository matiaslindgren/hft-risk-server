#include "format.h"
#include "risk_client.h"
#include <iostream>

int main(const int argc, const char *argv[]) {
  if (argc != 3) {
    std::cerr << rs::format("error: wrong number of args {} out of {}",
                            argc - 1, 2)
              << '\n';
    std::cerr << "usage: test server_address server_port\n";
    exit(2);
  }

  const std::string address = argv[1];
  const std::string port = argv[2];

  rs::RiskClient client(address, port);

  std::size_t order_counter = 0;

  enum class Instrument : unsigned {
    OurStock = 1,
    OtherStock = 2,
  };

  using namespace rs::protocol;

  auto check_response = [&client](const auto &order_id) {
    if (auto response = client.wait_for_response();
        response.status == OrderResponse::Status::ACCEPTED) {
      std::cout << rs::format("order {} accepted\n", order_id);
    } else {
      std::cout << rs::format("order {} rejected\n", order_id);
    }
  };

  {
    NewOrder new_order{NewOrder::MESSAGE_TYPE,
                       static_cast<uint64_t>(Instrument::OurStock),
                       ++order_counter,
                       10,
                       1,
                       'B'};
    client.send_message(new_order);
    check_response(order_counter);
  }
  {
    NewOrder new_order{NewOrder::MESSAGE_TYPE,
                       static_cast<uint64_t>(Instrument::OtherStock),
                       ++order_counter,
                       15,
                       1,
                       'S'};
    client.send_message(new_order);
    check_response(order_counter);
  }
  {
    NewOrder new_order{NewOrder::MESSAGE_TYPE,
                       static_cast<uint64_t>(Instrument::OtherStock),
                       ++order_counter,
                       4,
                       1,
                       'B'};
    client.send_message(new_order);
    check_response(order_counter);
  }
  {
    NewOrder new_order{NewOrder::MESSAGE_TYPE,
                       static_cast<uint64_t>(Instrument::OtherStock),
                       ++order_counter,
                       20,
                       1,
                       'B'};
    client.send_message(new_order);
    check_response(order_counter);
  }
  {
    Trade trade{Trade::MESSAGE_TYPE,
                static_cast<uint64_t>(Instrument::OtherStock), 1, 4, 1};
    client.send_message(trade);
  }
  {
    DeleteOrder delete_order{
        DeleteOrder::MESSAGE_TYPE,
        3,
    };
    client.send_message(delete_order);
  }
}
