#ifndef TX_TRADING_ENGINE_CORE_ERROR_BASE_HPP
#define TX_TRADING_ENGINE_CORE_ERROR_BASE_HPP

#include <fmt/format.h>

#include <system_error>
#include <type_traits>

namespace tx::core {

/// @brief 錯誤特徵交給個模組進行特化
template <typename T>
struct ErrorTraits;

template <typename E>
concept ErrorCodeEnum =
    std::is_enum_v<E> && std::is_error_code_enum_v<E> && requires(E ec) {
      { ErrorTraits<E>::make_code(ec) } -> std::same_as<std::error_code>;
    };

template <ErrorCodeEnum ErrcEnum>
class GenericError {
 private:
  std::error_code code_;
  int errno_;

  explicit GenericError(std::error_code code, int e = 0)
      : code_(code), errno_(e) {}

 public:
  // ===========================
  // 工廠方法
  // ===========================

  /// @brief 建立錯誤
  inline static GenericError from(ErrcEnum errc) noexcept {
    return GenericError(ErrorTraits<ErrcEnum>::make_code(errc));
  }

  /// @brief 建立錯誤並保存 errno
  inline static GenericError from(ErrcEnum errc, int e) noexcept {
    return GenericError(ErrorTraits<ErrcEnum>::make_code(errc), e);
  }

  // ===========================
  // 訪問器
  // ===========================

  /// @brief 取得底層的 std::error_code
  /// @return 標準錯誤碼
  [[nodiscard]] std::error_code code() const noexcept { return code_; }

  /// @brief 產生完整的錯誤訊息
  /// @return 格式化的錯誤訊息字串
  /// @details 格式範例："[tx.network.22]: Invalid argument"
  [[nodiscard]] std::string message() const noexcept {
    std::string header = fmt::format("[{}:{}]: {}", code_.category().name(),
                                     code_.value(), code_.message());

    if (errno_ == 0) {
      return header;
    } else {
      return fmt::format("{}\n └─▶ errno({}): {}", header, errno_,
                         std::generic_category().message(errno_));
    }
  }

  // ===========================
  // 錯誤檢查
  // ===========================
  /// @brief 檢查錯誤是否匹配特定錯誤碼
  [[nodiscard]] bool is(ErrcEnum ec) const noexcept {
    return code_ == ErrorTraits<ErrcEnum>::make_code(ec);
  }

  /// @brief 檢查錯誤是否匹配特定錯誤碼
  [[nodiscard]] bool is(std::errc ec) const noexcept {
    return code_ == std::make_error_code(ec);
  }
};

}  // namespace tx::core

#endif
