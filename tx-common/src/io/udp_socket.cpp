#include "tx/io/udp_socket.hpp"

#include <system_error>

#include "tx/error.hpp"

namespace tx::io {

Result<UdpSocket> UdpSocket::create() noexcept {
  auto socket = TRY(Socket::create_udp());

  return UdpSocket(std::move(socket));
}

Result<UdpSocket> UdpSocket::bind(const SocketAddress& local_addr) noexcept {
  auto socket = TRY(Socket::create_udp());

  CHECK(socket.bind(local_addr));

  return UdpSocket(std::move(socket));
}

}  // namespace tx::io
