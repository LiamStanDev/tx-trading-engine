#ifndef TX_TRADING_ENGINE_NETWORK_SOCKET_ADDRESS_HPP
#define TX_TRADING_ENGINE_NETWORK_SOCKET_ADDRESS_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <tx/core/result.hpp>
#include <tx/network/address_error.hpp>

namespace tx::network {

template <typename T>
using AddressResult = Result<T, AddressError>;

/// @brief Socket 地址封裝，用於方便切換網路序與主機序
/// @warning 尚未支持 IPv6
class SocketAddress {
 private:
  // 這邊不考慮 std::variant 因為我們要對其取地址
  union {
    sockaddr addr_;
    sockaddr_in addr4_;
    sockaddr_in6 addr6_;
  };
  socklen_t length_;

  SocketAddress() : length_(0) { std::memset(&addr6_, 0, sizeof(addr6_)); }

 public:
  /// @brief 從 IPv4 建立
  /// @param ip IP 地址字串 (e.g. "127.0.0.1")
  /// @param port 埠號 (主機位元序)
  static AddressResult<SocketAddress> from_ipv4(std::string_view ip,
                                                uint16_t port) noexcept;

  /// @brief 從字串建立 (格式: "IP:PORT")
  /// @param address 位址字串 (e.g. "127.0.0.1:8080")
  static AddressResult<SocketAddress> from_string(
      std::string_view address) noexcept;

  /// @brief 建立任意位置 (綁定所有網卡)
  /// @param port 埠號
  static SocketAddress any_ipv4(uint16_t port) noexcept;

  [[nodiscard]] bool is_ipv4() const noexcept {
    return addr_.sa_family == AF_INET;
  }

  [[nodiscard]] bool is_ipv6() const noexcept {
    return addr_.sa_family == AF_INET6;
  }

  /// @brief 取得底層結構 (給 POSIX API 使用)
  [[nodiscard]] const sockaddr* raw() const noexcept { return &addr_; }

  /// @brief 取得底層結構 (給 POSIX API 使用)
  [[nodiscard]] sockaddr* raw() noexcept { return &addr_; }

  [[nodiscard]] socklen_t length() const noexcept { return length_; }

  [[nodiscard]] socklen_t* length_ptr() noexcept { return &length_; }

  /// @brief 取得 IPv4 地址結構（用於 Multicast）
  /// @return in_addr 指標，如果不是 IPv4 則返回 nullptr
  [[nodiscard]] const struct in_addr* ipv4_addr() const noexcept {
    if (addr_.sa_family == AF_INET) {
      return &addr4_.sin_addr;
    }
    return nullptr;
  }

  /// @brief 取得埠號（主機位元組序）
  [[nodiscard]] uint16_t port() const noexcept;

  /// @brief 轉換為字串
  [[nodiscard]] std::string to_string() const noexcept;
};
}  // namespace tx::network

#endif
