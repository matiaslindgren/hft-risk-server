#include "tcp.h"
#include "format.h"
#include "logging.h"

namespace rs::tcp {

auto logger = rs::logging::make_logger("tcp", rs::logging::Level::INFO);

[[nodiscard]] inline std::string ip_to_string(const addrinfo *ai) {
  std::string result;

  switch (ai->ai_family) {
  case AF_INET: {
    char buffer[INET_ADDRSTRLEN];
    const auto *sa = reinterpret_cast<const sockaddr_in *>(ai->ai_addr);
    inet_ntop(AF_INET, &sa->sin_addr, buffer, INET_ADDRSTRLEN);
    result.assign(buffer);
    break;
  }
  case AF_INET6: {
    char buffer[INET6_ADDRSTRLEN];
    const auto *sa6 = reinterpret_cast<const sockaddr_in6 *>(ai->ai_addr);
    inet_ntop(AF_INET6, &sa6->sin6_addr, buffer, INET6_ADDRSTRLEN);
    result.assign(buffer);
    break;
  }
  default: {
    break;
  }
  }

  if (result.empty()) {
    throw std::runtime_error(
        rs::format("IP string conversion failed: ", std::strerror(errno)));
  }

  return result;
}

[[nodiscard]] inline auto get_address_info(const std::string &address,
                                           const std::string &port) {
  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  // Prefer IPv4 or IPv6 automatically
  hints.ai_family = AF_UNSPEC;
  // Use streaming socket (will be TCP for IP)
  hints.ai_socktype = SOCK_STREAM;

  // Get all addresses available for use
  addrinfo *address_info;
  if (int error =
          getaddrinfo(address.c_str(), port.c_str(), &hints, &address_info);
      error != 0) {
    throw std::runtime_error(
        rs::format("Failed to call getaddrinfo: {}", gai_strerror(error)));
  }
  return std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{address_info,
                                                            &freeaddrinfo};
}

[[nodiscard]] inline std::pair<int, std::string>
create_and_bind_socket(const addrinfo *address_info) {
  // Find a valid address, create a socket, and bind address to socket
  for (auto *ai = address_info; ai != nullptr; ai = ai->ai_next) {
    logger->debug("Trying address: '{}'", ip_to_string(ai));

    // Create socket
    int socket_fd = socket(ai->ai_family, ai->ai_socktype, 0);
    if (socket_fd < 0) {
      logger->debug("Socket creation failed: {}", std::strerror(errno));
      continue;
    }

    // Bind an address to the socket
    if (int status = bind(socket_fd, ai->ai_addr, ai->ai_addrlen); status < 0) {
      logger->debug("Unable to bind address to socket {}: {}", socket_fd,
                    std::strerror(errno));
      close(socket_fd);
      continue;
    }

    // Success, return socket fd and its IP address
    return {socket_fd, ip_to_string(ai)};
  }

  throw std::runtime_error(
      rs::format("Unable to find a valid address, cannot bind socket"));
}

[[nodiscard]] inline std::pair<int, std::string>
create_and_connect_socket(const addrinfo *address_info) {
  // Create a client socket and connect to a server socket.
  for (auto *ai = address_info; ai != nullptr; ai = ai->ai_next) {
    logger->debug("Trying address: '{}'", ip_to_string(ai));

    // Create socket
    int socket_fd = socket(ai->ai_family, ai->ai_socktype, 0);
    if (socket_fd < 0) {
      logger->debug("Socket creation failed: {}", std::strerror(errno));
      continue;
    }

    // Connect
    if (int status = connect(socket_fd, ai->ai_addr, ai->ai_addrlen);
        status < 0) {
      logger->debug("Unable to connect socket {}: {}", socket_fd,
                    std::strerror(errno));
      close(socket_fd);
      continue;
    }

    // Success, return socket fd and its IP address
    return {socket_fd, ip_to_string(ai)};
  }

  throw std::runtime_error(
      rs::format("Unable to find a valid address for connecting socket"));
}

void Socket::try_listen() {
  if (int status = listen(fd, backlog_length); status < 0) {
    throw std::runtime_error(
        rs::format("Unable to listen: {}", std::strerror(errno)));
  }
}

void Socket::close_fd() noexcept {
  if (fd != -1) {
    logger->debug("Closing socket {}", fd);
    close(fd);
    fd = -1;
  }
}

Server::Server(const std::string &bind_address, const std::string &port) {
  logger->info("Server binding to {}:{}", bind_address, port);
  auto address_info = get_address_info(bind_address, port);
  auto [socket_fd, got_address] = create_and_bind_socket(address_info.get());
  socket_ = Socket{socket_fd};
  socket_.try_listen();
  logger->debug("Server socket {} bound to {} and is now listening", socket_.fd,
                got_address);
}

[[nodiscard]] protocol::Message
Server::receive_message(const Socket &socket) const {
  logger->debug("Server reading message from socket {}", socket.fd);
  std::string buffer(msg_buffer_length, '\0');
  auto msg_length = recv(socket.fd, buffer.data(), buffer.size(), 0);
  if (msg_length < 0) {
    throw std::runtime_error(
        rs::format("Failed reading message from socket {}: {}", socket.fd,
                   std::strerror(errno)));
  }
  logger->info("Server received message of length {}", msg_length);
  return buffer.substr(0, msg_length);
}

std::size_t Server::send_message(const Socket &socket,
                                 const protocol::Message &msg) const {
  logger->debug("Server sending message of length {} to socket {}", msg.size(),
                socket.fd);
  auto msg_length = send(socket.fd, msg.data(), msg.size(), 0);
  if (msg_length < 0) {
    throw std::runtime_error(
        rs::format("Failed sending message to socket {}: {}", socket.fd,
                   std::strerror(errno)));
  }
  logger->info("Server sent message of length {} to socket {}", msg_length,
               socket.fd);
  return msg_length;
}

[[nodiscard]] Socket Server::next_connection() {
  sockaddr_storage client_addr;
  auto sin_size = static_cast<socklen_t>(sizeof(client_addr));
  auto new_fd =
      accept(socket_.fd, reinterpret_cast<sockaddr *>(&client_addr), &sin_size);
  if (new_fd < 0) {
    throw std::runtime_error(
        rs::format("Failed accepting new connection to socket {}: {}",
                   socket_.fd, std::strerror(errno)));
  }
  return Socket{new_fd};
}

Client::Client(const std::string &server_addr, const std::string &port) {
  logger->debug("Client connecting to {}:{}", server_addr, port);
  auto address_info = get_address_info(server_addr, port);
  auto [socket_fd, got_address] = create_and_connect_socket(address_info.get());
  socket_ = Socket{socket_fd};
  logger->info("Socket {} connected to '{}'", socket_.fd, got_address);
}

[[nodiscard]] protocol::Message Client::receive_message() const {
  logger->debug("Client reading server response");
  std::string buffer(msg_buffer_length, '\0');
  auto msg_length = recv(socket_.fd, buffer.data(), buffer.size(), 0);
  if (msg_length < 0) {
    throw std::runtime_error(
        rs::format("Failed reading message from socket {}: {}", socket_.fd,
                   std::strerror(errno)));
  }
  return buffer.substr(0, msg_length);
}

std::size_t Client::send_message(const protocol::Message &msg) const {
  logger->debug("Client sending message of size {}", msg.size());
  auto msg_length = send(socket_.fd, msg.data(), msg.size(), 0);
  if (msg_length < 0) {
    throw std::runtime_error(
        rs::format("Failed sending message to socket {}: {}", socket_.fd,
                   std::strerror(errno)));
  }
  return msg_length;
}

} // namespace rs::tcp
