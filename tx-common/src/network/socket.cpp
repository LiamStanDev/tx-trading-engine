#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <tx/core/result.hpp>
#include <tx/network/socket.hpp>
#include <tx/network/socket_address.hpp>
#include <utility>

#include "tx/network/socket_error.hpp"

namespace tx::network {

SocketResult<Socket> Socket::create_tcp() noexcept {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::CreateFailed));
  }

  return Ok(Socket{fd});
}

SocketResult<Socket> Socket::create_udp() noexcept {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::CreateFailed));
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

SocketResult<> Socket::bind(const SocketAddress& addr) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  if (::bind(fd_, addr.raw(), addr.length()) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::BindFailed));
  }

  return Ok<>();
}

SocketResult<> Socket::listen(int backlog) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  if (::listen(fd_, backlog) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::ListenFailed));
  }

  return Ok<>();
}

SocketResult<Socket> Socket::accept(SocketAddress* client_addr) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  int client_fd;

  if (client_addr) {
    client_fd = ::accept(fd_, client_addr->raw(), client_addr->length_ptr());
  } else {
    client_fd = ::accept(fd_, nullptr, nullptr);
  }
  if (client_fd < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::AcceptFailed));
  }

  return Ok(Socket{client_fd});
}

SocketResult<> Socket::connect(const SocketAddress& addr) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  if (::connect(fd_, addr.raw(), addr.length()) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::ConnectFailed));
  }
  return Ok<>();
}

SocketResult<> Socket::set_nonblocking(bool enable) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::FcntlGetFailed));
  }

  if (enable) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (::fcntl(fd_, F_SETFL, flags) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::FcntlSetFailed));
  }

  return Ok<>();
}

SocketResult<> Socket::set_tcp_keepalive(bool enable) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) <
      0) {
    return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
  }
  return Ok<>();
}

SocketResult<size_t> Socket::send(std::span<const std::byte> data) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  ssize_t n = ::send(fd_, data.data(), data.size(), 0);

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // EAGAIN/EWOULDBLOCK: 非阻塞模式下緩衝區滿了
      // 這非錯誤，而是稍後重試
      return Err(SocketError::from_errno(SocketErrorCode::WouldBlock,
                                         "Send buffer full"));
    }

    // EINTR: 被信號中斷
    // EPIPE: 對端關閉連線 (寫入已關閉的 socket)
    return Err(SocketError::from_errno(SocketErrorCode::AlreadyClosed));
  }

  // 發送時對端關閉
  if (n == 0 && data.size() > 0) {
    return Err(SocketError{SocketErrorCode::AlreadyClosed,
                           "send() returned 0 unexpectedly"});
  }

  return Ok(static_cast<size_t>(n));
}

SocketResult<size_t> Socket::recv(std::span<std::byte> buffer) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  ssize_t n = ::recv(fd_, buffer.data(), buffer.size(), 0);

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // EAGAIN/EWOULDBLOCK: 非阻塞模式下沒有數據
      // 這非錯誤，而是稍後重試
      return Err(SocketError::from_errno(SocketErrorCode::WouldBlock,
                                         "No data available"));
    }

    return Err(SocketError::from_errno(SocketErrorCode::RecvFailed));
  }

  return Ok(static_cast<size_t>(n));
}

SocketResult<size_t> Socket::sendto(std::span<const std::byte> data,
                                    const SocketAddress& dest) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  ssize_t n =
      ::sendto(fd_, data.data(), data.size(), 0, dest.raw(), dest.length());

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return Err(SocketError::from_errno(SocketErrorCode::WouldBlock,
                                         "Send buffer full"));
    }
    return Err(SocketError::from_errno(SocketErrorCode::SendFailed));
  }

  // UDP 無流量控制，通常為全部發送不然就是失敗
  return Ok(static_cast<size_t>(n));
}

SocketResult<size_t> Socket::recvfrom(std::span<std::byte> buffer,
                                      SocketAddress* src) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
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
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // EAGAIN/EWOULDBLOCK: 非阻塞模式下沒有數據
      // 這非錯誤，而是稍後重試
      return Err(SocketError::from_errno(SocketErrorCode::WouldBlock,
                                         "No data available"));
    }

    return Err(SocketError::from_errno(SocketErrorCode::RecvFailed));
  }

  return Ok(static_cast<size_t>(n));
}

SocketResult<> Socket::set_reuseaddr(bool enable) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }
  int optval = enable ? 1 : 0;
  if (enable) {
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
        0) {
      return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
    }
  }
  return Ok<>();
}

SocketResult<> Socket::set_tcp_nodelay(bool enable) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) <
      0) {
    return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
  }
  return Ok<>();
}

SocketResult<> Socket::set_recv_buffer_size(int size) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
  }
  return Ok<>();
}

SocketResult<> Socket::join_multicast_group(
    const SocketAddress& multicast_addr,
    const SocketAddress& interface_addr) noexcept {
  const auto* mcast_in_addr = multicast_addr.ipv4_addr();
  if (!mcast_in_addr) {
    return Err(SocketError{SocketErrorCode::InvalidParameter,
                           "Multicast address must be IPv4"});
  }

  const auto* iface_in_addr = interface_addr.ipv4_addr();
  if (!iface_in_addr) {
    return Err(SocketError{SocketErrorCode::InvalidParameter,
                           "Interface address must be IPv4"});
  }

  // 驗證 Multicast 地址範圍（224.0.0.0 ~ 239.255.255.255）
  uint32_t addr_host = ntohl(mcast_in_addr->s_addr);
  if (addr_host < 0xE0000000 || addr_host > 0xEFFFFFFF) {
    return Err(SocketError{SocketErrorCode::InvalidParameter,
                           "Address " + multicast_addr.to_string() +
                               " is not in Multicast range (224.0.0.0/4)"});
  }

  struct ip_mreq mreq{};
  mreq.imr_multiaddr = *mcast_in_addr;
  mreq.imr_interface = *iface_in_addr;

  if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    return Err(SocketError::from_errno(SocketErrorCode::JoinMulticastFailed));
  }

  return Ok<>();
}
/// /// @brief 離開 Multicast group
SocketResult<> Socket::leave_multicast_group(
    const SocketAddress& multicast_addr,
    const SocketAddress& interface_addr) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  const auto* mcast_in_addr = multicast_addr.ipv4_addr();
  if (!mcast_in_addr) {
    return Err(SocketError{SocketErrorCode::InvalidParameter,
                           "Multicast address must be IPv4"});
  }

  const auto* iface_in_addr = interface_addr.ipv4_addr();
  if (!iface_in_addr) {
    return Err(SocketError{SocketErrorCode::InvalidParameter,
                           "Interface address must be IPv4"});
  }

  // 驗證 Multicast 地址範圍（224.0.0.0 ~ 239.255.255.255）
  uint32_t addr_host = ntohl(mcast_in_addr->s_addr);
  if (addr_host < 0xE0000000 || addr_host > 0xEFFFFFFF) {
    return Err(SocketError{SocketErrorCode::InvalidParameter,
                           "Address " + multicast_addr.to_string() +
                               " is not in Multicast range (224.0.0.0/4)"});
  }

  struct ip_mreq mreq{};
  mreq.imr_multiaddr = *mcast_in_addr;
  mreq.imr_interface = *iface_in_addr;

  if (::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    return Err(SocketError::from_errno(SocketErrorCode::LeaveMulticastFailed));
  }

  return Ok<>();
}

/// @brief 設定 Multicast TTL (發送端)
SocketResult<> Socket::set_multicast_ttl(int ttl) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  if (ttl < 0 || ttl > 255) {
    return Err(
        SocketError{SocketErrorCode::InvalidParameter,
                    "Multicast TTL must be 0-255, got " + std::to_string(ttl)});
  }

  if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
  }

  return Ok<>();
}

/// /// @brief 設定是否接收自己發送的 Multicast 封包
SocketResult<> Socket::set_multicast_loopback(bool enable) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  int loop = enable ? 1 : 0;

  if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) <
      0) {
    return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
  }
  return Ok<>();
}

SocketResult<> Socket::set_send_buffer_size(int size) noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::SetSockOptFailed));
  }
  return Ok<>();
}

SocketResult<SocketAddress> Socket::local_address() const noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  SocketAddress addr = SocketAddress::any_ipv4(0);  // 臨時物件
  if (::getsockname(fd_, addr.raw(), addr.length_ptr()) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::GetSockNameFailed));
  }
  return Ok(addr);
}

SocketResult<SocketAddress> Socket::remote_address() const noexcept {
  if (!is_valid()) {
    return Err(SocketError{SocketErrorCode::InvalidState});
  }

  SocketAddress addr = SocketAddress::any_ipv4(0);  // 臨時物件
  if (::getpeername(fd_, addr.raw(), addr.length_ptr()) < 0) {
    return Err(SocketError::from_errno(SocketErrorCode::GetPeerNameFailed));
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
