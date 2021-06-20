#include "format.h"
#include "risk_service.h"
#include <iostream>

int main(const int argc, const char *argv[]) {
  if (argc != 5) {
    std::cerr << rs::format("error: wrong number of args {} out of {}",
                            argc - 1, 4)
              << '\n';
    std::cerr << "usage: risk_service ip_address tcp_port max_buy_position "
                 "max_sell_position\n";
    exit(2);
  }

  const std::string address{argv[1]};
  const std::string port{argv[2]};
  const long long max_buy_pos{std::stoll(argv[3])};
  const long long max_sell_pos{std::stoll(argv[4])};

  rs::RiskService service(address, port, max_buy_pos, max_sell_pos);
  service.wait();
}
