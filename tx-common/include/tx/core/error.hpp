#ifndef TX_TRADING_ENGINE_CORE_ERROR_HPP
#define TX_TRADING_ENGINE_CORE_ERROR_HPP

#include <string_view>
#include <system_error>

namespace tx::core {

/// @brief 統一的錯誤類型，封裝系統錯誤碼與上下文資訊
/// @details
/// Error 是一個輕量級的錯誤包裝類，用於在無異常環境下傳遞錯誤資訊。
/// 它基於標準庫的 std::error_code，並額外提供：
/// - Context 字串：用於描述錯誤發生的上下文或額外細節
/// - 工廠方法：方便從不同來源建立錯誤（errno、std::errc、自訂 error_code）
/// - 型別安全檢查：透過 is() 方法判斷特定錯誤類型
///
/// @note 此類設計為 move-only，所有字串參數以值傳遞以支援完美轉發
/// ```
class Error {
 private:
  std::error_code code_;  ///< 系統錯誤碼
  std::string context_;   ///< 錯誤上下文描述

  /// @brief 私有建構子（僅供工廠方法使用）
  explicit Error(std::error_code code) noexcept : code_(code) {}

  /// @brief 私有建構子（僅供工廠方法使用）
  explicit Error(std::error_code code, std::string context) noexcept
      : code_(code), context_(std::move(context)) {}

 public:
  // ===========================
  // 工廠方法
  // ===========================

  /// @brief 從當前 errno 建立錯誤（system_category）
  /// @return 包含當前 errno 值的 Error
  /// @note 呼叫時機：系統調用失敗後立即呼叫，以捕獲正確的 errno
  static Error from_errno() noexcept;

  /// @brief 從當前 errno 建立錯誤並附加上下文
  /// @param context 錯誤上下文描述（如 "Failed to bind socket"）
  /// @return 包含當前 errno 值與上下文的 Error
  static Error from_errno(std::string context) noexcept;

  /// @brief 從 std::error_code 建立錯誤
  /// @param code 標準錯誤碼
  /// @return 包含該錯誤碼的 Error
  static Error from_code(std::error_code code) noexcept;

  /// @brief 從 std::error_code 建立錯誤並附加上下文
  /// @param code 標準錯誤碼
  /// @param context 錯誤上下文描述
  /// @return 包含該錯誤碼與上下文的 Error
  static Error from_code(std::error_code code, std::string context) noexcept;

  /// @brief 從 std::errc 建立錯誤（generic_category）
  /// @param ec 標準錯誤條件枚舉
  /// @return 包含該錯誤條件的 Error
  /// @note 適用於非系統調用錯誤（如參數驗證失敗）
  static Error from_errc(std::errc ec) noexcept;

  /// @brief 從 std::errc 建立錯誤並附加上下文
  /// @param ec 標準錯誤條件枚舉
  /// @param context 錯誤上下文描述
  /// @return 包含該錯誤條件與上下文的 Error
  static Error from_errc(std::errc ec, std::string context) noexcept;

  // ===========================
  // RAII
  // ===========================
  Error(const Error& other) = delete;
  Error& operator=(const Error& other) = delete;
  Error(Error&& other) = default;
  Error& operator=(Error&& other) = default;

  // ===========================
  // 訪問器
  // ===========================

  /// @brief 取得底層的 std::error_code
  /// @return 標準錯誤碼
  [[nodiscard]] std::error_code code() const noexcept { return code_; }

  /// @brief 取得錯誤上下文字串
  /// @return 上下文描述的 string_view（可能為空）
  [[nodiscard]] std::string_view context() const noexcept { return context_; }

  /// @brief 產生完整的錯誤訊息（包含錯誤碼與上下文）
  /// @return 格式化的錯誤訊息字串
  /// @details 格式範例："[generic.22]: Invalid argument (Port must be 1-65535)"
  [[nodiscard]] std::string message() const noexcept;

  // ===========================
  // 錯誤檢查
  // ===========================

  /// @brief 檢查錯誤是否匹配特定錯誤碼
  /// @tparam Errc 錯誤碼枚舉類型（需滿足 std::is_error_code_enum）
  /// @param ec 要比對的錯誤碼
  /// @return 若匹配則回傳 true
  /// @note 支援任何定義了 make_error_code() 的錯誤枚舉類型
  ///
  /// @example
  /// ```cpp
  /// if (error.is(std::errc::bad_file_descriptor)) {
  ///   // 處理無效的檔案描述符
  /// }
  /// ```
  template <typename Errc>
    requires std::is_error_code_enum_v<Errc>
  [[nodiscard]] bool is(Errc ec) const noexcept {
    return code_ == make_error_code(ec);
  }
};

}  // namespace tx::core

namespace tx {
using Error = core::Error;

}  // namespace tx

#endif
