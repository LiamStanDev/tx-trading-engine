#ifndef TX_TRADING_ENGINE_IO_SOCKET_ADDRESS_HPP
#define TX_TRADING_ENGINE_IO_SOCKET_ADDRESS_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "tx/error.hpp"

namespace tx::io {

/// @brief Socket 地址封裝，用於方便切換網路序與主機序
///
/// @note 不支持 IPv6
///
class SocketAddress {
 private:
  // 這邊不考慮 std::variant 因為我們要對齊取地址
  union {
    sockaddr addr_;
    sockaddr_in addr4_;
  };
  socklen_t length_ = 0;

 public:
  // ----------------------------------------------------------------------------
  // Constructors
  // ----------------------------------------------------------------------------

  /// @brief 建立未初始化的 IPv4 地址
  ///
  /// @note 只有有賦值 AF_INET
  SocketAddress() noexcept;

  // ----------------------------------------------------------------------------
  // Factory Methods
  // ----------------------------------------------------------------------------

  /// @brief 建立 IPv6 的默認地址值
  ///
  // static SocketAddress default_ipv6() noexcept;

  /// @brief 從 IPv4 建立
  /// @param ip IP 地址字串 (e.g. "127.0.0.1")
  /// @param port 埠號 (主機位元序)
  ///
  static Result<SocketAddress> from(std::string_view ip,
                                    uint16_t port) noexcept;

  /// @brief 從字串建立 (格式: "IP:PORT")
  /// @param address 位址字串 (e.g. "127.0.0.1:8080")
  ///
  static Result<SocketAddress> from(std::string_view address) noexcept;

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  /// @brief 建立任意位置 (綁定所有網卡)
  /// @param port 埠號
  ///
  static SocketAddress any(uint16_t port) noexcept;

  /// @brief 取得底層結構 (給 POSIX API 使用)
  ///
  [[nodiscard]] const sockaddr* raw() const noexcept { return &addr_; }

  /// @brief 取得底層結構 (給 POSIX API 使用)
  ///
  [[nodiscard]] sockaddr* raw() noexcept { return &addr_; }

  [[nodiscard]] socklen_t length() const noexcept { return length_; }

  [[nodiscard]] socklen_t* length_ptr() noexcept { return &length_; }

  /// @brief 取得 IPv4 地址結構（用於 Multicast）
  /// @return in_addr 指標
  ///
  [[nodiscard]] const struct in_addr* addr_ptr() const noexcept {
    return &addr4_.sin_addr;
  }

  /// @brief 取得 ip 字符串
  ///
  [[nodiscard]] std::string ip() const noexcept;

  /// @brief 取得埠號（主機位元組序）
  ///
  [[nodiscard]] uint16_t port() const noexcept;

  /// @brief 轉換為字串
  ///
  [[nodiscard]] std::string to_string() const noexcept;
};
}  // namespace tx::io

#endif
