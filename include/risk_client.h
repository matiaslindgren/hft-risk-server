#ifndef INCLUDED_RISKSERVICE_CLIENT_HEADER
#define INCLUDED_RISKSERVICE_CLIENT_HEADER
/*
 * Risk client that sends messages to the risk server.
 */

#include "logging.h"
#include "tcp.h"
#include <algorithm>
#include <string>

namespace rs {

auto logger = logging::make_logger("risk_client", logging::Level::INFO);

class RiskClient {
  using PayloadSize = decltype(protocol::Header::payloadSize);
  using SequenceNum = decltype(protocol::Header::sequenceNumber);

public:
  explicit RiskClient(const std::string &server_address,
                      const std::string &server_port)
      : tcp_client_(server_address, server_port) {}

  template <typename Payload> void send_message(const Payload &payload) {
    logger->info("Sending message of type {} to risk server",
                 payload.messageType);
    protocol::Header header{
        payload.messageType,
        static_cast<PayloadSize>(sizeof(payload)),
        next_package_id(),
        now(),
    };
    auto sent_size =
        tcp_client_.send_message(protocol::encode(header, payload));
    logger->debug("Sent {} bytes to risk server", sent_size);
  }

  protocol::OrderResponse wait_for_response() const {
    logger->info("Reading response from risk server");
    auto msg = tcp_client_.receive_message();
    logger->debug("Got message of length {}", msg.length());
    auto header = protocol::decode_header(msg);
    if (header.version != protocol::OrderResponse::MESSAGE_TYPE) {
      logger->error("Unknown message type {} received from risk server",
                    header.version);
      return {};
    }
    return protocol::decode_payload<protocol::OrderResponse>(msg);
  }

private:
  tcp::Client tcp_client_;
  SequenceNum package_counter_{0};

  SequenceNum next_package_id() { return ++package_counter_; }
};

} // namespace rs

#endif // INCLUDED_RISKSERVICE_CLIENT_HEADER
