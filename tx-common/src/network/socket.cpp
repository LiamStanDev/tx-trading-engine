#include "tx/network/socket.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include "tx/core/error.hpp"
#include "tx/core/result.hpp"
#include "tx/network/socket_address.hpp"

namespace tx::network {

Result<Socket> Socket::create_tcp() noexcept {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return Err(Error::from_errno("Failed to create TCP socket"));
  }

  return Ok(Socket{fd});
}

Result<Socket> Socket::create_udp() noexcept {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return Err(Error::from_errno("Failed to create UDP socket"));
  }

  return Ok(Socket{fd});
}

Socket::~Socket() noexcept { close(); }

Socket::Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
  if (this != &other) {
    close();
    fd_ = std::exchange(other.fd_, -1);
  }
  return *this;
}

Result<> Socket::bind(const SocketAddress& addr) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  if (::bind(fd_, addr.raw(), addr.length()) < 0) {
    return Err(Error::from_errno("Failed to bind socket"));
  }

  return Ok<>();
}

Result<> Socket::listen(int backlog) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  if (::listen(fd_, backlog) < 0) {
    return Err(Error::from_errno("Failed to listen on socket"));
  }

  return Ok<>();
}

Result<Socket> Socket::accept(SocketAddress* client_addr) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  int client_fd;

  if (client_addr) {
    client_fd = ::accept(fd_, client_addr->raw(), client_addr->length_ptr());
  } else {
    client_fd = ::accept(fd_, nullptr, nullptr);
  }
  if (client_fd < 0) {
    return Err(Error::from_errno("Failed to accept connection"));
  }

  return Ok(Socket{client_fd});
}

Result<> Socket::connect(const SocketAddress& addr) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  if (::connect(fd_, addr.raw(), addr.length()) < 0) {
    return Err(Error::from_errno("Failed to connect socket"));
  }
  return Ok<>();
}

Result<> Socket::set_nonblocking(bool enable) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0) {
    return Err(Error::from_errno("Failed to get socket flags (fcntl F_GETFL)"));
  }

  if (enable) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (::fcntl(fd_, F_SETFL, flags) < 0) {
    return Err(Error::from_errno("Failed to set socket flags (fcntl F_SETFL)"));
  }

  return Ok<>();
}

Result<> Socket::set_tcp_keepalive(bool enable) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) <
      0) {
    return Err(Error::from_errno("Failed to set TCP keepalive"));
  }
  return Ok<>();
}

Result<size_t> Socket::send(std::span<const std::byte> data) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  ssize_t n = ::send(fd_, data.data(), data.size(), 0);

  if (n < 0) {
    if (errno == EAGAIN) {
      // EAGAIN/EWOULDBLOCK: 非阻塞模式下緩衝區滿了 (linux 下相同)
      // 這非錯誤，而是稍後重試
      return Err(Error::from_errno("Send buffer full (would block)"));
    }

    // EINTR: 被信號中斷
    // EPIPE: 對端關閉連線 (寫入已關閉的 socket)
    return Err(Error::from_errno("Failed to send data"));
  }

  // 發送時對端關閉
  if (n == 0 && data.size() > 0) {
    return Err(Error::from_errc(std::errc::connection_reset,
                                "send() returned 0 unexpectedly"));
  }

  return Ok(static_cast<size_t>(n));
}

Result<size_t> Socket::recv(std::span<std::byte> buffer) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  ssize_t n = ::recv(fd_, buffer.data(), buffer.size(), 0);

  if (n < 0) {
    if (errno == EAGAIN) {
      // EAGAIN/EWOULDBLOCK: 非阻塞模式下沒有數據 (linux 下相同)
      // 這非錯誤，而是稍後重試
      return Err(Error::from_errno("No data available (would block)"));
    }

    return Err(Error::from_errno("Failed to receive data"));
  }

  return Ok(static_cast<size_t>(n));
}

Result<size_t> Socket::sendto(std::span<const std::byte> data,
                              const SocketAddress& dest) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  ssize_t n =
      ::sendto(fd_, data.data(), data.size(), 0, dest.raw(), dest.length());

  if (n < 0) {
    if (errno == EAGAIN) {
      return Err(Error::from_errno("Send buffer full (would block)"));
    }
    return Err(Error::from_errno("Failed to send data (sendto)"));
  }

  // UDP 無流量控制，通常為全部發送不然就是失敗
  return Ok(static_cast<size_t>(n));
}

Result<size_t> Socket::recvfrom(std::span<std::byte> buffer,
                                SocketAddress* src) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  ssize_t n;

  // 調用者想知道發送對端地址
  if (src) {
    n = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, src->raw(),
                   src->length_ptr());
  } else {
    n = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, nullptr, nullptr);
  }

  if (n < 0) {
    if (errno == EAGAIN) {
      // EAGAIN/EWOULDBLOCK: 非阻塞模式下沒有數據
      // 這非錯誤，而是稍後重試
      return Err(Error::from_errno("No data available (would block)"));
    }

    return Err(Error::from_errno("Failed to receive data (recvfrom)"));
  }

  return Ok(static_cast<size_t>(n));
}

Result<> Socket::set_reuseaddr(bool enable) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }
  int optval = enable ? 1 : 0;
  if (enable) {
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
        0) {
      return Err(Error::from_errno("Failed to set SO_REUSEADDR"));
    }
  }
  return Ok<>();
}

Result<> Socket::set_tcp_nodelay(bool enable) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) <
      0) {
    return Err(Error::from_errno("Failed to set TCP_NODELAY"));
  }
  return Ok<>();
}

Result<> Socket::set_recv_buffer_size(int size) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
    return Err(Error::from_errno("Failed to set receive buffer size"));
  }
  return Ok<>();
}

Result<> Socket::join_multicast_group(
    const SocketAddress& multicast_addr,
    const SocketAddress& interface_addr) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  const auto* mcast_in_addr = multicast_addr.ipv4_addr();
  if (!mcast_in_addr) {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "Multicast address must be IPv4"));
  }

  const auto* iface_in_addr = interface_addr.ipv4_addr();
  if (!iface_in_addr) {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "Interface address must be IPv4"));
  }

  // 驗證 Multicast 地址範圍（224.0.0.0 ~ 239.255.255.255）
  uint32_t addr_host = ntohl(mcast_in_addr->s_addr);
  if (addr_host < 0xE0000000 || addr_host > 0xEFFFFFFF) {
    return Err(
        Error::from_errc(std::errc::invalid_argument,
                         "Address " + multicast_addr.to_string() +
                             " is not in Multicast range (224.0.0.0/4)"));
  }

  struct ip_mreq mreq{};
  mreq.imr_multiaddr = *mcast_in_addr;
  mreq.imr_interface = *iface_in_addr;

  if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    return Err(Error::from_errno("Failed to join multicast group"));
  }

  return Ok<>();
}

Result<> Socket::leave_multicast_group(
    const SocketAddress& multicast_addr,
    const SocketAddress& interface_addr) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  const auto* mcast_in_addr = multicast_addr.ipv4_addr();
  if (!mcast_in_addr) {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "Multicast address must be IPv4"));
  }

  const auto* iface_in_addr = interface_addr.ipv4_addr();
  if (!iface_in_addr) {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "Interface address must be IPv4"));
  }

  // 驗證 Multicast 地址範圍（224.0.0.0 ~ 239.255.255.255）
  uint32_t addr_host = ntohl(mcast_in_addr->s_addr);
  if (addr_host < 0xE0000000 || addr_host > 0xEFFFFFFF) {
    return Err(
        Error::from_errc(std::errc::invalid_argument,
                         "Address " + multicast_addr.to_string() +
                             " is not in Multicast range (224.0.0.0/4)"));
  }

  struct ip_mreq mreq{};
  mreq.imr_multiaddr = *mcast_in_addr;
  mreq.imr_interface = *iface_in_addr;

  if (::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    return Err(Error::from_errno("Failed to leave multicast group"));
  }

  return Ok<>();
}

Result<> Socket::set_multicast_ttl(int ttl) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  if (ttl < 0 || ttl > 255) {
    return Err(Error::from_errc(
        std::errc::invalid_argument,
        "Multicast TTL must be 0-255, got " + std::to_string(ttl)));
  }

  if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
    return Err(Error::from_errno("Failed to set multicast TTL"));
  }

  return Ok<>();
}

Result<> Socket::set_multicast_loopback(bool enable) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  int loop = enable ? 1 : 0;

  if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) <
      0) {
    return Err(Error::from_errno("Failed to set multicast loopback"));
  }
  return Ok<>();
}

Result<> Socket::set_send_buffer_size(int size) noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
    return Err(Error::from_errno("Failed to set send buffer size"));
  }
  return Ok<>();
}

Result<SocketAddress> Socket::local_address() const noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  SocketAddress addr = SocketAddress::any_ipv4(0);  // 臨時物件
  if (::getsockname(fd_, addr.raw(), addr.length_ptr()) < 0) {
    return Err(Error::from_errno("Failed to get socket local address"));
  }
  return Ok(addr);
}

Result<SocketAddress> Socket::remote_address() const noexcept {
  if (!is_valid()) {
    return Err(Error::from_errc(std::errc::bad_file_descriptor,
                                "Socket is in invalid state"));
  }

  SocketAddress addr = SocketAddress::any_ipv4(0);  // 臨時物件
  if (::getpeername(fd_, addr.raw(), addr.length_ptr()) < 0) {
    return Err(Error::from_errno("Failed to get socket remote address"));
  }
  return Ok(addr);
}

void Socket::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

int Socket::release() noexcept {
  int old_fd = fd_;
  fd_ = -1;
  return old_fd;
}

}  // namespace tx::network
