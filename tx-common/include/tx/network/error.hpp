#ifndef TX_TRADING_ENGINE_NETWORK_CONTEXT_ERROR_HPP
#define TX_TRADING_ENGINE_NETWORK_CONTEXT_ERROR_HPP

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include "tx/core/result.hpp"

namespace tx::network {

enum class NetworkErrc : uint8_t {
  // Address Error
  INVALID_ADDRESS = 1,
  ADDRESS_FAMILY_NOT_SUPPORTED,
  INVALID_PORT,

  // Socket Error
  SOCKET_CREATE_FAILED,
  INVALID_SOCKET,
  BIND_FAILED,
  LISTEN_FAILED,
  ACCEPT_FAILED,
  CONNECT_FAILED,
  CONNECT_IN_PROGRESS,
  SET_SOCKETOPT_FAILED,
  WOULDBLOCK,
  SEND_FAILED,
  RECV_FAILED,
  INVALID_MULTICAST_ADDR,
  INVALID_INTERFACE_ADDR,
  JOIN_MULTICAST_FAILED,
  LEAVE_MULTICAST_FAILED,
  INVALID_TTL,
  GET_SOCKET_NAME_FAILED,
  GET_PEER_NAME_FAILED,

  // Connection State
  CONNECTION_CLOSED,
  CONNECTION_RESET,
  BROKEN_PIPE,
};

class NetworkCategory : public std::error_category {
  const char* name() const noexcept override { return "tx.network"; }
  std::string message(int ec) const override;
};

const NetworkCategory& network_category() noexcept;

std::error_code make_error_code(NetworkErrc ec) {
  return std::error_code{static_cast<int>(ec), network_category()};
}

class NetworkError {
 private:
  std::error_code code_;
  int errno_;

  explicit NetworkError(std::error_code code, int e = 0)
      : code_(code), errno_(e) {}

 public:
  // ===========================
  // 工廠方法
  // ===========================

  /// @brief 建立錯誤
  static NetworkError from(NetworkErrc errc) noexcept;

  /// @brief 建立錯誤並保存 errno
  static NetworkError from(NetworkErrc errc, int e) noexcept;

  // ===========================
  // 訪問器
  // ===========================

  /// @brief 取得底層的 std::error_code
  /// @return 標準錯誤碼
  [[nodiscard]] std::error_code code() const noexcept { return code_; }

  /// @brief 產生完整的錯誤訊息
  /// @return 格式化的錯誤訊息字串
  /// @details 格式範例："[tx.network.22]: Invalid argument"
  [[nodiscard]] std::string message() const noexcept;

  // ===========================
  // 錯誤檢查
  // ===========================
  /// @brief 檢查錯誤是否匹配特定錯誤碼
  [[nodiscard]] bool is(NetworkErrc ec) const noexcept {
    return code_ == make_error_code(ec);
  }

  /// @brief 檢查錯誤是否匹配特定錯誤碼
  [[nodiscard]] bool is(std::errc ec) const noexcept {
    return code_ == std::make_error_code(ec);
  }
};

template <typename T = void>
using Result = tx::Result<T, NetworkError>;

}  // namespace tx::network

template <>
struct std::is_error_code_enum<tx::network::NetworkErrc> : std::true_type {};

#endif
