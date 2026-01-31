#include "tx/io/socket_address.hpp"

#include <arpa/inet.h>
#include <fmt/format.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <charconv>
#include <cstring>
#include <string>
#include <string_view>

#include "tx/error.hpp"

namespace tx::io {

SocketAddress::SocketAddress() noexcept {
  std::memset(&addr4_, 0, sizeof(sockaddr_in));
  addr4_.sin_family = AF_INET;
  length_ = sizeof(sockaddr_in);
}

Result<SocketAddress> SocketAddress::from(std::string_view ip,
                                          uint16_t port) noexcept {
  SocketAddress addr;
  addr.addr4_.sin_port = htons(port);

  // NOTE: 與 C API 交互的字串要使用 NULL TERMINATED
  char ip_buf[INET_ADDRSTRLEN];  // IPv4 最大長度: "255.255.255.255\0" = 16
  if (ip.size() >= sizeof(ip_buf)) {
    return tx::fail(std::errc::invalid_argument, "Invliad IPv4 length");
  }
  std::memcpy(ip_buf, ip.data(), ip.size());
  ip_buf[ip.size()] = '\0';  // 這樣比較快
  if (inet_pton(AF_INET, ip_buf, &addr.addr4_.sin_addr) != 1) {
    // 他有設定 errno 但是沒什麼用
    return tx::fail(std::errc::invalid_argument, "Invalid IPv4 format");
  }
  return addr;
}

Result<SocketAddress> SocketAddress::from(std::string_view address) noexcept {
  // 嘗試 IPv4
  auto colon_pos = address.find(':');
  if (colon_pos == std::string_view::npos) {
    return tx::fail(std::errc::invalid_argument, "Invalid port format");
  }

  auto ip = address.substr(0, colon_pos);
  auto port_str = address.substr(colon_pos + 1);

  uint16_t port = 0;
  auto [ptr, ec] =
      std::from_chars(port_str.data(), port_str.data() + port_str.size(), port);

  if (ec != std::errc{}) {
    return tx::fail(std::errc::invalid_argument, "Port parse failed");
  }

  // 檢查是否完全解析 (防止 "8080abc" 情況發生)
  if (ptr != port_str.data() + port_str.size()) {
    return tx::fail(std::errc::invalid_argument, "Port parse failed");
  }

  return from(ip, port);
}

SocketAddress SocketAddress::any(uint16_t port) noexcept {
  SocketAddress addr;
  addr.addr4_.sin_port = htons(port);
  addr.addr4_.sin_addr.s_addr = INADDR_ANY;
  return addr;
}

uint16_t SocketAddress::port() const noexcept { return ntohs(addr4_.sin_port); }

std::string SocketAddress::ip() const noexcept {
  char ip_buf[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &addr4_.sin_addr, ip_buf, sizeof(ip_buf));
  return std::string(ip_buf);
}

std::string SocketAddress::to_string() const noexcept {
  char ip_buf[INET_ADDRSTRLEN];

  inet_ntop(AF_INET, &addr4_.sin_addr, ip_buf, sizeof(ip_buf));
  return std::string(ip_buf) + ":" + std::to_string(port());
}

}  // namespace tx::io
