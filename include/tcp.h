#ifndef INCLUDED_RISKSERVICE_TCP_HEADER
#define INCLUDED_RISKSERVICE_TCP_HEADER
/*
 * TCP server and client.
 */

extern "C" {
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}

#include "protocol.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace rs::tcp {

constexpr std::size_t msg_buffer_length = (1ull << 16) - 1;

// Extract IPv4 or IPv6 address from addrinfo* data as a string.
[[nodiscard]] inline std::string ip_to_string(const addrinfo *);

// Get valid IPv4 and IPv6 address data to which we can bind the given port.
[[nodiscard]] inline auto get_address_info(const std::string &,
                                           const std::string & = "");

// SERVER:
//   Create a socket and bind an address to it.
//   Return the socket's file descriptor and bound address.
[[nodiscard]] inline std::pair<int, std::string>
create_and_bind_socket(const addrinfo *);

// CLIENT:
//   Create a socket and connect to an address.
//   Return the socket's file descriptor and bound address.
[[nodiscard]] inline std::pair<int, std::string>
create_and_connect_socket(const addrinfo *);

// UNIX socket with a file descriptor.
// The file descriptor is closed on destruction.
class Socket {

public:
  static constexpr std::size_t backlog_length = 1 << 8;

  int fd;

  explicit Socket(int fd = -1) : fd(fd) {}

  ~Socket() noexcept { close_fd(); }

  // Disallow copy semantics.
  // We don't want two objects managing the same socket.
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  // Allow move semantics.
  // We simply need to move the file descriptor id, without closing the file.
  Socket(Socket &&other) noexcept : fd(other.fd) { other.fd = -1; }
  Socket &operator=(Socket &&other) noexcept {
    using std::swap;
    if (fd != other.fd) {
      close_fd();
      swap(fd, other.fd);
    }
    return *this;
  }

  // Start listening on socket at fd.
  void try_listen();

private:
  // Close file descriptor or do nothing if it is -1.
  void close_fd() noexcept;
};

class Server {

public:
  static constexpr std::size_t read_buffer_length = (1ull << 8) - 1;

  explicit Server(const std::string &, const std::string &);

  // Rule of five.
  ~Server() noexcept = default;

  // Disallow copy semantics.
  // We don't want two Server objects listening to the same socket.
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  // Allow move semantics.
  // Default, memberwise move of the Socket object is sufficient.
  Server(Server &&other) noexcept = default;
  Server &operator=(Server &&other) noexcept = default;

  // Read incoming message from client.
  [[nodiscard]] protocol::Message receive_message(const Socket &) const;

  // Send response to client and get sent length.
  std::size_t send_message(const Socket &, const protocol::Message &) const;

  // Accept a new connection and return a Socket object for it.
  [[nodiscard]] Socket next_connection();

private:
  Socket socket_;
};

class Client {

public:
  explicit Client(const std::string &, const std::string &);

  // Same constraints as in rs::tcp::Server.
  ~Client() noexcept = default;

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  Client(Client &&other) noexcept = default;
  Client &operator=(Client &&other) noexcept = default;

  // Read order response from socket.
  [[nodiscard]] protocol::Message receive_message() const;

  // Send message to socket and get sent length.
  std::size_t send_message(const protocol::Message &) const;

private:
  Socket socket_;
};

} // namespace rs::tcp

#endif // INCLUDED_RISKSERVICE_TCP_HEADER
