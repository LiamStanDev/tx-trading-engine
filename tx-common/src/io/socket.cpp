#include "tx/io/socket.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <utility>

#include "tx/error.hpp"

namespace tx::io {

Result<Socket> Socket::create_tcp() noexcept {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return tx::fail(errno, "socket() failed");
  }

  return Socket(fd);
}

Result<Socket> Socket::create_udp() noexcept {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return tx::fail(errno);
  }

  return Socket(fd);
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
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  if (::bind(fd_, addr.raw(), addr.length()) < 0) {
    return tx::fail(errno, "bind() failed");
  }

  return {};
}

Result<> Socket::listen(int backlog) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  if (::listen(fd_, backlog) < 0) {
    return tx::fail(errno, "listen() failed");
  }

  return {};
}

Result<Socket> Socket::accept(SocketAddress* client_addr) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  int client_fd;

  if (client_addr) {
    do {
      client_fd = ::accept(fd_, client_addr->raw(), client_addr->length_ptr());
    } while (client_fd < 0 && errno == EINTR);
  } else {
    do {
      client_fd = ::accept(fd_, nullptr, nullptr);
    } while (client_fd < 0 && errno == EINTR);
  }
  if (client_fd < 0) {
    return tx::fail(errno, "accept() failed");
  }

  return Socket(client_fd);
}

Result<> Socket::connect(const SocketAddress& addr) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  int ret;
  do {
    ret = ::connect(fd_, addr.raw(), addr.length());
  } while (ret < 0 && errno == EINTR);

  if (ret < 0) {
    return tx::fail(errno, "connect() failed");
  }
  return {};
}

Result<> Socket::set_nonblocking(bool enable) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags < 0) {
    return tx::fail(errno);
  }

  if (enable) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (::fcntl(fd_, F_SETFL, flags) < 0) {
    return tx::fail(errno);
  }

  return {};
}

Result<> Socket::set_tcp_keepalive(bool enable) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) <
      0) {
    return tx::fail(errno);
  }
  return {};
}

Result<size_t> Socket::send(std::span<const std::byte> data) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }
  ssize_t n;

  do {
    n = ::send(fd_, data.data(), data.size(), 0);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno);
  }

  return static_cast<size_t>(n);
}

Result<size_t> Socket::recv(std::span<std::byte> buffer) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  ssize_t n;
  do {
    n = ::recv(fd_, buffer.data(), buffer.size(), 0);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno);
  }

  return static_cast<size_t>(n);
}

Result<size_t> Socket::sendto(std::span<const std::byte> data,
                              const SocketAddress& dest) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  ssize_t n;

  do {
    n = ::sendto(fd_, data.data(), data.size(), 0, dest.raw(), dest.length());
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno, "sendto() failed");
  }

  // UDP 無流量控制,通常為全部發送不然就是失敗
  return static_cast<size_t>(n);
}

Result<size_t> Socket::recvfrom(std::span<std::byte> buffer,
                                SocketAddress* src) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  ssize_t n;

  // 調用者想知道發送對端地址
  if (src) {
    do {
      n = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, src->raw(),
                     src->length_ptr());
    } while (n < 0 && errno == EINTR);
  } else {
    do {
      n = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, nullptr, nullptr);
    } while (n < 0 && errno == EINTR);
  }

  if (n < 0) {
    return tx::fail(errno, "recvfrom() failed");
  }

  return static_cast<size_t>(n);
}

Result<> Socket::set_reuseaddr(bool enable) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
      0) {
    return tx::fail(errno, "setsockopt(SO_REUSEADDR) failed");
  }
  return {};
}

Result<> Socket::set_tcp_nodelay(bool enable) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }
  int optval = enable ? 1 : 0;
  if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) <
      0) {
    return tx::fail(errno, "setsockopt(TCP_NODELAY) failed");
  }
  return {};
}

Result<> Socket::set_recv_buffer_size(int size) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
    return tx::fail(errno, "setsockopt(SO_RCVBUF) failed");
  }
  return {};
}

Result<> Socket::join_multicast_group(
    const SocketAddress& multicast_addr,
    const SocketAddress& interface_addr) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  const auto* mcast_in_addr = multicast_addr.ipv4_addr();
  if (!mcast_in_addr) {
    return tx::fail(std::errc::invalid_argument, "Invalid multicast address");
  }

  // 驗證 Multicast 地址範圍（224.0.0.0 ~ 239.255.255.255）
  uint32_t addr_host = ntohl(mcast_in_addr->s_addr);
  if (addr_host < 0xE0000000 || addr_host > 0xEFFFFFFF) {
    return tx::fail(std::errc::invalid_argument, "Invalid multicast address");
  }

  const auto* iface_in_addr = interface_addr.ipv4_addr();
  if (!iface_in_addr) {
    return tx::fail(std::errc::invalid_argument, "Invalid interface address");
  }

  struct ip_mreq mreq{};
  mreq.imr_multiaddr = *mcast_in_addr;
  mreq.imr_interface = *iface_in_addr;

  if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    return tx::fail(errno, "setsockopt(IP_ADD_MEMBERSHIP) failed");
  }

  return {};
}

Result<> Socket::leave_multicast_group(
    const SocketAddress& multicast_addr,
    const SocketAddress& interface_addr) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  const auto* mcast_in_addr = multicast_addr.ipv4_addr();
  if (!mcast_in_addr) {
    return tx::fail(std::errc::invalid_argument, "Invalid multicast address");
  }

  // 驗證 Multicast 地址範圍（224.0.0.0 ~ 239.255.255.255）
  uint32_t addr_host = ntohl(mcast_in_addr->s_addr);
  if (addr_host < 0xE0000000 || addr_host > 0xEFFFFFFF) {
    return tx::fail(std::errc::invalid_argument, "Invalid multicast address");
  }

  const auto* iface_in_addr = interface_addr.ipv4_addr();
  if (!iface_in_addr) {
    return tx::fail(std::errc::invalid_argument, "Invalid interface address");
  }

  struct ip_mreq mreq{};
  mreq.imr_multiaddr = *mcast_in_addr;
  mreq.imr_interface = *iface_in_addr;

  if (::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) <
      0) {
    return tx::fail(errno, "setsockopt(IP_DROP_MEMBERSHIP) failed");
  }

  return {};
}

Result<> Socket::set_multicast_ttl(int ttl) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  if (ttl < 0 || ttl > 255) {
    return tx::fail(std::errc::invalid_argument,
                    "Invalid TTL (should be 0 ~ 225)");
  }

  if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
    return tx::fail(errno, "setsockopt(IP_MULTICAST_TTL) failed");
  }

  return {};
}

Result<> Socket::set_multicast_loopback(bool enable) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  int loop = enable ? 1 : 0;

  if (::setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) <
      0) {
    return tx::fail(errno, "setsockopt(IP_MULTICAST_LOOP) failed");
  }
  return {};
}

Result<> Socket::set_send_buffer_size(int size) noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  if (::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
    return tx::fail(errno, "setsockopt(SO_SNDBUF) failed");
  }
  return {};
}

Result<SocketAddress> Socket::local_address() const noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  SocketAddress addr = SocketAddress::any_ipv4(0);  // 臨時物件
  if (::getsockname(fd_, addr.raw(), addr.length_ptr()) < 0) {
    return tx::fail(errno, "getsockname() failed");
  }
  return addr;
}

Result<SocketAddress> Socket::remote_address() const noexcept {
  if (!is_valid()) {
    return tx::fail(std::errc::bad_file_descriptor, "Invalid socket");
  }

  SocketAddress addr = SocketAddress::any_ipv4(0);  // 臨時物件
  if (::getpeername(fd_, addr.raw(), addr.length_ptr()) < 0) {
    return tx::fail(errno, "getpeername() failed");
  }
  return addr;
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

}  // namespace tx::io
