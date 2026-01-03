#ifndef TX_TRADING_ENGINE_IPC_ERROR_HPP
#define TX_TRADING_ENGINE_IPC_ERROR_HPP

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include "tx/core/result.hpp"

namespace tx::ipc {

enum class IpcErrc : uint8_t {
  INVALID_SHM_NAME,
  INVALID_SHM_SIZE,
  SHM_EXISTED,
  SHM_CREATE_FAILED,
  SHM_OPEN_FAILED,
  SHM_NOT_FOUND,
  SHM_PERMISSION_DENY,
};

class IpcCategory : public std::error_category {
  const char* name() const noexcept override { return "tx.ipc"; }
  std::string message(int) const override;
};

const IpcCategory& ipc_category() noexcept;

std::error_code make_error_code(IpcErrc ec) noexcept;

class IpcError {
 private:
  std::error_code code_;
  int errno_;

  explicit IpcError(std::error_code code, int e = 0) : code_(code), errno_(e) {}

 public:
  // ===========================
  // 工廠方法
  // ===========================

  /// @brief 建立錯誤
  static IpcError from(IpcErrc errc) noexcept;

  /// @brief 建立錯誤並保存 errno
  static IpcError from(IpcErrc errc, int e) noexcept;

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
  [[nodiscard]] bool is(IpcErrc ec) const noexcept {
    return code_ == make_error_code(ec);
  }

  /// @brief 檢查錯誤是否匹配特定錯誤碼
  [[nodiscard]] bool is(std::errc ec) const noexcept {
    return code_ == std::make_error_code(ec);
  }
};

template <typename T = void>
using Result = tx::Result<T, IpcError>;

}  // namespace tx::ipc

template <>
struct std::is_error_code_enum<tx::ipc::IpcErrc> : std::true_type {};

#endif
