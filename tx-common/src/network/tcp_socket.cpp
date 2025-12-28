#include <tx/network/tcp_socket.hpp>

#include "tx/core/result.hpp"
#include "tx/network/socket.hpp"

namespace tx::network {
SocketResult<TcpSocket> TcpSocket::connect(const SocketAddress& remote_addr,
                                           bool nodelay) noexcept {
  auto socket = TRY(Socket::create_tcp());

  if (nodelay) {
    auto result = socket.set_tcp_nodelay(true);
    if (!result) {
      // 非致命錯誤，還是可以繼續運行
      // TODO: logger
    }
  }

  CHECK(socket.connect(remote_addr));

  return Ok(TcpSocket{std::move(socket)});
}

SocketResult<TcpSocket> TcpSocket::serve(const SocketAddress& local_addr,
                                         int backlog) noexcept {
  auto socket = TRY(Socket::create_tcp());

  CHECK(socket.set_reuseaddr(true));
  CHECK(socket.bind(local_addr));
  CHECK(socket.listen(backlog));

  return Ok(TcpSocket{std::move(socket)});
}

SocketResult<TcpSocket> TcpSocket::accept(SocketAddress* client_addr) noexcept {
  auto socket = TRY(socket_.accept(client_addr));

  return Ok(TcpSocket{std::move(socket)});
}

}  // namespace tx::network
