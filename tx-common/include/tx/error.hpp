#ifndef TX_TRADING_ENGINE_ERROR_HPP
#define TX_TRADING_ENGINE_ERROR_HPP

#include <expected>
#include <source_location>
#include <system_error>

namespace tx {

/// @brief 將 POSIX errno 轉為標準 C++ error_code 類型
/// @param ev 系統 errno
/// @return 包含系統 category 的 error_code
/// @warning ec 只能為 errno, 否則沒有意義
///
std::error_code make_error_code(int ev) noexcept;

/// @brief 錯誤發生上下文資訊
///
struct ErrorContext {
  std::error_code ec;             ///< 發生時原始錯誤碼
  std::source_location location;  ///< 發生時錯誤位置
  const char* message;            ///< 上下文說明 (靜態字串)
  bool is_active = false;         ///< 標記是否有資料
};

/// @biref 執行緒局部的錯誤診斷暫存器
/// @note 每個執行緒獨立管理其最後一次錯誤路徑。
///
class ErrorRegistry {
 private:
  static inline thread_local ErrorContext last_info{};

 public:
  /// @brief 捕捉錯誤源頭
  ///
  static void capture_origin(std::error_code ec, const char* msg,
                             std::source_location loc) noexcept;

  /// @brief 獲取當前執行緒最後一次發生的錯誤詳情
  ///
  static const ErrorContext& get_last_error() noexcept { return last_info; }

  /// @brief 重置錯誤上下文狀態
  ///
  static void clear() noexcept { last_info.is_active = false; }
};

/// @brief 可能失敗調用的標準回傳類型別名
/// @tparam T 成功的數值類型，預設為 void
///
template <typename T = void>
using Result = std::expected<T, std::error_code>;

/// @brief 產生錯誤結果並觸發診斷資訊捕捉
/// @param ec 錯誤代碼
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
/// @warning 此函式應僅在錯誤源頭呼叫。
///
[[nodiscard]] inline auto fail(
    std::error_code ec, const char* msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  ErrorRegistry::capture_origin(ec, msg, loc);
  return std::unexpected(ec);
}

/// @brief 產生錯誤結果並觸發診斷資訊捕捉
/// @param ev errno
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
/// @warning 此函式應僅在錯誤源頭呼叫。
///
[[nodiscard]] inline auto fail(
    int ev, const char* msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = tx::make_error_code(ev);
  ErrorRegistry::capture_origin(ec, msg, loc);
  return std::unexpected(ec);
}

/// @brief 產生錯誤結果並觸發診斷資訊捕捉
/// @param ev 錯誤代碼 (std::errc)
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
/// @warning 此函式應僅在錯誤源頭呼叫。
///
[[nodiscard]] inline auto fail(
    std::errc ev, const char* msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = std::make_error_code(ev);
  ErrorRegistry::capture_origin(ec, msg, loc);
  return std::unexpected(ec);
}

}  // namespace tx

/// @brief 解包 std::expected<T, E>，失敗時提前返回
/// @details 使用 GNU Statement Expression 提供類似 Rust ? operator 的語法
/// @warning 只能在返回 std::expected<T, E> 的函數中使用
///
/// @example
///   auto socket = TRY(Socket::create_tcp());
///   auto addr = TRY(SocketAddress::from_ipv4("127.0.0.1", 8080));
///
#define TRY(expr)                                                   \
  __extension__({                                                   \
    auto&& _res = (expr);                                           \
                                                                    \
    static_assert(                                                  \
        requires {                                                  \
          _res.error();                                             \
          _res.has_value();                                         \
        }, "TRY() can only be used with std::expected-like types"); \
                                                                    \
    if (!_res) [[unlikely]] {                                       \
      return std::unexpected(std::move(_res.error()));              \
    }                                                               \
                                                                    \
    std::move(*_res);                                               \
  })

/// @brief 檢查 std::expected<void, E>，失敗時提前返回
/// 用於不需要返回值的場景
/// @warning 只能在返回 std::expected<T, E> 的函數中使用
///
/// @example
///   CHECK(socket.connect(addr));
///   CHECK(socket.set_nonblocking(true));
///
#define CHECK(expr)                                                   \
  do {                                                                \
    auto&& _res = (expr);                                             \
                                                                      \
    static_assert(                                                    \
        requires {                                                    \
          _res.error();                                               \
          _res.has_value();                                           \
        }, "CHECK() can only be used with std::expected-like types"); \
                                                                      \
    if (!_res) [[unlikely]] {                                         \
      return std::unexpected(_res.error());                           \
    }                                                                 \
  } while (0)

#endif
