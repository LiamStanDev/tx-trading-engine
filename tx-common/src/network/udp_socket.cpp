#include <tx/network/udp_socket.hpp>

namespace tx::network {

SocketResult<UdpSocket> UdpSocket::create() noexcept {
  auto socket = TRY(Socket::create_udp());

  return Ok(UdpSocket{std::move(socket)});
}

SocketResult<UdpSocket> UdpSocket::bind(
    const SocketAddress& local_addr) noexcept {
  auto socket = TRY(Socket::create_udp());

  CHECK(socket.bind(local_addr));

  return Ok(UdpSocket{std::move(socket)});
}

}  // namespace tx::network
