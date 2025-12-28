#include <arpa/inet.h>
#include <fmt/format.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <tx/network/socket_address.hpp>

namespace tx::network {

AddressResult<SocketAddress> SocketAddress::from_ipv4(std::string_view ip,
                                                      uint16_t port) noexcept {
  SocketAddress addr;
  addr.addr4_.sin_family = AF_INET;
  addr.addr4_.sin_port = htons(port);

  // NOTE: 與 C API 交互的字串要使用 NULL TERMINATED
  char ip_buf[INET_ADDRSTRLEN];  // IPv4 最大長度: "255.255.255.255\0" = 16
  if (ip.size() >= sizeof(ip_buf)) {
    return Err(AddressError{AddressErrorCode::InvalidFormat, std::string{ip}});
  }
  std::memcpy(ip_buf, ip.data(), ip.size());
  ip_buf[ip.size()] = '\0';  // 這樣比較快
  if (inet_pton(AF_INET, ip_buf, &addr.addr4_.sin_addr) != 1) {
    return Err(AddressError{AddressErrorCode::InvalidFormat, std::string{ip}});
  }
  addr.length_ = sizeof(sockaddr_in);
  return Ok(addr);
}

AddressResult<SocketAddress> SocketAddress::from_string(
    std::string_view address) noexcept {
  // IPv6 檢測（目前不支援）
  if (!address.empty() && address.front() == '[') {
    return Err(AddressError{AddressErrorCode::UnsupportedFamily,
                            "IPv6 not yet supported"});
  }

  // 嘗試 IPv4
  auto colon_pos = address.find(':');
  if (colon_pos == std::string_view::npos) {
    return Err(AddressError{AddressErrorCode::ParseFailure,
                            std::string{address} + " (missing port)"});
  }

  auto ip = address.substr(0, colon_pos);
  auto port_str = address.substr(colon_pos + 1);

  uint16_t port = 0;
  auto [ptr, ec] =
      std::from_chars(port_str.data(), port_str.data() + port_str.size(), port);

  if (ec != std::errc{}) {
    return Err(
        AddressError{AddressErrorCode::InvalidPort, std::string{port_str}});
  }

  // 檢查是否完全解析 (防止 "8080abc" 情況發生)
  if (ptr != port_str.data() + port_str.size()) {
    return Err(AddressError{
        AddressErrorCode::InvalidPort,
        fmt::format("{} (contains non-digit characters)", port_str)});
  }

  return from_ipv4(ip, port);
}

SocketAddress SocketAddress::any_ipv4(uint16_t port) noexcept {
  SocketAddress addr;
  addr.addr4_.sin_family = AF_INET;
  addr.addr4_.sin_port = htons(port);
  addr.addr4_.sin_addr.s_addr = INADDR_ANY;
  addr.length_ = sizeof(sockaddr_in);
  return addr;
}

uint16_t SocketAddress::port() const noexcept {
  if (addr_.sa_family == AF_INET) {
    return ntohs(addr4_.sin_port);
  } else {
    return ntohs(addr6_.sin6_port);
  }
}

std::string SocketAddress::to_string() const noexcept {
  char ip_buf[INET6_ADDRSTRLEN];

  if (addr_.sa_family == AF_INET) {
    inet_ntop(AF_INET, &addr4_.sin_addr, ip_buf, sizeof(ip_buf));
    return std::string(ip_buf) + ":" + std::to_string(port());
  } else if (addr_.sa_family == AF_INET6) {
    inet_ntop(AF_INET6, &addr6_.sin6_addr, ip_buf, sizeof(ip_buf));
    return fmt::format("[{}]: {}", ip_buf, port());
  }

  return "unknown";
}

}  // namespace tx::network
