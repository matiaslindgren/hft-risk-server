#ifndef INCLUDED_RISKSERVICE_PROTOCOL_HEADER
#define INCLUDED_RISKSERVICE_PROTOCOL_HEADER
/*
 * Message protocol with serialization.
 */

#include "format.h"
#include <cstdint>
#include <ctime>
#include <string>

namespace rs::protocol {

struct Header {
  uint16_t version;        // Protocol version
  uint16_t payloadSize;    // Payload size in bytes
  uint32_t sequenceNumber; // Sequence number for this package
  uint64_t timestamp;      // Timestamp, number of nanoseconds from Unix epoch.
};

struct NewOrder {
  static constexpr uint16_t MESSAGE_TYPE = 1;
  uint16_t messageType;   // Message type of this message
  uint64_t listingId;     // Financial instrument id associated to this message
  uint64_t orderId;       // Order id used for further order changes
  uint64_t orderQuantity; // Order quantity
  uint64_t orderPrice;    // Order price, the price contains 4 implicit decimals
  char side;              // The side of the order, 'B' for buy and 'S' for sell
};

struct DeleteOrder {
  static constexpr uint16_t MESSAGE_TYPE = 2;
  uint16_t messageType; // Message type of this message
  uint64_t orderId;     // Order id that refers to the original order id
};

struct ModifyOrderQuantity {
  static constexpr uint16_t MESSAGE_TYPE = 3;
  uint16_t messageType; // Message type of this message
  uint64_t orderId;     // Order id that refers to the original order id
  uint64_t newQuantity; // The new quantity
};

struct Trade {
  static constexpr uint16_t MESSAGE_TYPE = 4;
  uint16_t messageType;   // Message type of this message
  uint64_t listingId;     // Financial instrument id associated to this message
  uint64_t tradeId;       // Order id that refers to the original order id
  uint64_t tradeQuantity; // Trade quantity
  uint64_t tradePrice;    // Trade price, the price contains 4 implicit decimals
};

struct OrderResponse {
  static constexpr uint16_t MESSAGE_TYPE = 5;
  enum class Status : uint16_t {
    ACCEPTED = 0,
    REJECTED = 1,
  };
  uint16_t messageType; // Message type of this message
  uint64_t orderId;     // Order id that refers to the original order id
  Status status;        // Status of the order
};

using Message = std::string;

// Return a stateful, callable parser that splits given msg into parts,
// separated by space. Calling the parser returns one 64 bit unsigned int, until
// all parts have been parsed.
inline auto make_parser(const Message &msg) {
  auto parse_next = [&msg, begin = std::size_t{0},
                     end = std::size_t{0}]() mutable {
    if (end >= msg.length()) {
      throw std::runtime_error(
          "Unable to parse next value, reached end of message");
    }
    end = msg.find(' ', begin);
    if (end == msg.npos) {
      end = msg.length();
    }
    auto res = msg.substr(begin, end - begin);
    begin = end + 1;
    return std::stoull(res);
  };
  return parse_next;
}

// Decoders.
// All messages are parsed into 64 bit unsigned ints and then casted to correct
// message field type.

inline Header decode_header(const Message &new_order) {
  auto parse_next = make_parser(new_order);
  Header h{
      static_cast<decltype(h.version)>(parse_next()),
      static_cast<decltype(h.payloadSize)>(parse_next()),
      static_cast<decltype(h.sequenceNumber)>(parse_next()),
      static_cast<decltype(h.timestamp)>(parse_next()),
  };
  return h;
}

template <typename Payload> inline Payload decode_payload(const Message &);

template <> inline NewOrder decode_payload(const Message &msg) {
  auto parse_next = make_parser(msg);
  // Skip header.
  for (auto i = 0; i < 4; ++i) {
    parse_next();
  }
  NewOrder p{
      static_cast<decltype(p.messageType)>(parse_next()),
      static_cast<decltype(p.listingId)>(parse_next()),
      static_cast<decltype(p.orderId)>(parse_next()),
      static_cast<decltype(p.orderQuantity)>(parse_next()),
      static_cast<decltype(p.orderPrice)>(parse_next()),
      static_cast<decltype(p.side)>(parse_next()),
  };
  return p;
}

template <> inline DeleteOrder decode_payload(const Message &msg) {
  auto parse_next = make_parser(msg);
  for (auto i = 0; i < 4; ++i) {
    parse_next();
  }
  DeleteOrder p{
      static_cast<decltype(p.messageType)>(parse_next()),
      static_cast<decltype(p.orderId)>(parse_next()),
  };
  return p;
}

template <> inline ModifyOrderQuantity decode_payload(const Message &msg) {
  auto parse_next = make_parser(msg);
  for (auto i = 0; i < 4; ++i) {
    parse_next();
  }
  ModifyOrderQuantity p{
      static_cast<decltype(p.messageType)>(parse_next()),
      static_cast<decltype(p.orderId)>(parse_next()),
      static_cast<decltype(p.newQuantity)>(parse_next()),
  };
  return p;
}

template <> inline Trade decode_payload(const Message &msg) {
  auto parse_next = make_parser(msg);
  for (auto i = 0; i < 4; ++i) {
    parse_next();
  }
  Trade p{
      static_cast<decltype(p.messageType)>(parse_next()),
      static_cast<decltype(p.listingId)>(parse_next()),
      static_cast<decltype(p.tradeId)>(parse_next()),
      static_cast<decltype(p.tradePrice)>(parse_next()),
      static_cast<decltype(p.tradeQuantity)>(parse_next()),
  };
  return p;
}

template <> inline OrderResponse decode_payload(const Message &msg) {
  auto parse_next = make_parser(msg);
  for (auto i = 0; i < 4; ++i) {
    parse_next();
  }
  OrderResponse p{
      static_cast<decltype(p.messageType)>(parse_next()),
      static_cast<decltype(p.orderId)>(parse_next()),
      static_cast<decltype(p.status)>(parse_next()),
  };
  return p;
}

// Encoders.
// All field values converted to string with std::to_string.
// The resulting encoding is a concatenation of all string values, separated by
// space.

inline Message encode_header(const Header &h) {
  return rs::format("{} {} {} {}", h.version, h.payloadSize, h.sequenceNumber,
                    h.timestamp);
}

template <typename Payload>
inline Message encode(const Header &h, const Payload &p) {
  return encode_header(h) + " " + encode(p);
}

inline Message encode(const NewOrder &p) {
  return rs::format("{} {} {} {} {} {}", p.messageType, p.listingId, p.orderId,
                    p.orderQuantity, p.orderPrice, p.side);
}

inline Message encode(const DeleteOrder &p) {
  return rs::format("{} {}", p.messageType, p.orderId);
}

inline Message encode(const ModifyOrderQuantity &p) {
  return rs::format("{} {} {}", p.messageType, p.orderId, p.newQuantity);
}

inline Message encode(const Trade &p) {
  return rs::format("{} {} {} {} {}", p.messageType, p.listingId, p.tradeId,
                    p.tradeQuantity, p.tradePrice);
}

inline Message encode(const OrderResponse &p) {
  return rs::format("{} {} {}", p.messageType, p.orderId,
                    static_cast<uint16_t>(p.status));
}

} // namespace rs::protocol

namespace rs {

using ListingID = decltype(protocol::NewOrder::listingId);
using OrderID = decltype(protocol::NewOrder::orderId);
using Quantity = decltype(protocol::NewOrder::orderQuantity);
using Timestamp = decltype(protocol::Header::timestamp);

inline Timestamp now() { return static_cast<Timestamp>(std::time(nullptr)); }

} // namespace rs

#endif // INCLUDED_RISKSERVICE_PROTOCOL_HEADER
