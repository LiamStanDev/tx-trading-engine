#include "onyx/io/udp_socket.hpp"

#include "onyx/error.hpp"

namespace onyx::io {

UdpSocket::UdpSocket(Socket socket) noexcept : socket_(std::move(socket)) {}

Result<UdpSocket> UdpSocket::create() noexcept {
  auto socket = TRY(Socket::create_udp());

  return UdpSocket(std::move(socket));
}

Result<UdpSocket> UdpSocket::bind(const SocketAddress& local_addr) noexcept {
  auto socket = TRY(Socket::create_udp());

  CHECK(socket.bind(local_addr));

  return UdpSocket(std::move(socket));
}

}  // namespace onyx::io
