#ifndef ONYX_ERROR_HPP
#define ONYX_ERROR_HPP

#include <cxxabi.h>
#include <fmt/format.h>
#include <stdint.h>

#include <expected>
#include <source_location>
#include <system_error>

#include "execinfo.h"

namespace onyx {

/// @brief 錯誤發生上下文資訊
struct alignas(64) ErrorContext {
  static inline constexpr size_t MAX_FRAME_COUNT = 6;

  std::error_code ec;                 ///< 發生時原始錯誤碼
  std::source_location location;      ///< 發生時錯誤位置
  uint8_t frame_count;                ///< 實際捕捉到的調用棧幀數
  void* stackframe[MAX_FRAME_COUNT];  ///< 調用棧幀位置陣列
  std::string message;                ///< 上下文說明

  friend bool operator==(const ErrorContext& lhs, const std::error_code& rhs) noexcept {
    return lhs.ec == rhs;
  }

  friend bool operator==(const ErrorContext& lhs, const std::errc& rhs) noexcept {
    return lhs.ec == rhs;
  }
};

/// @brief 錯誤標記，可用於查詢錯誤資訊
/// 用於 std::expected<T, ErrorToken> 異常返回值
struct ErrorToken {
  const ErrorContext& context() const noexcept;
  std::error_code ec() const noexcept;
  std::string message() const noexcept;

  friend bool operator==(const ErrorToken& lhs, const std::error_code& rhs) noexcept {
    return lhs.context().ec == rhs;
  }

  friend bool operator==(const ErrorToken& lhs, const std::errc& rhs) noexcept {
    return lhs.context().ec == rhs;
  }
};

static_assert(sizeof(ErrorToken) == 1, "Token must fit in a single byte register");

/// @brief 執行緒局部的錯誤診斷儲存槽
/// 用來保存 ErrorContext 內容，不透過 `std::expected` 的 `E`
/// 返回大量數據，降低返回值成本
///
/// @note 每個執行緒獨立管理其最後一次錯誤路徑。
class ErrorRegistry {
 public:
 private:
  static inline thread_local ErrorContext ctx_{};

 public:
  static const ErrorContext& get() noexcept { return ctx_; }

  /// @brief 錯誤捕捉 - 含完整調用棧
  /// 完整記錄錯誤現場，內部調用 `backtrace` 保存堆疊資訊，*成本較高*。
  /// 適用於*非預期*或*系統/邏輯崩潰*性質錯誤。
  ///
  /// 適用於「非預期」或「系統/邏輯崩潰」性質的錯誤
  /// 當此類錯誤發生時，通常意味著程式狀態已損壞或環境異常，需保留詳細路徑以供事後診斷
  /// (Post-mortem analysis)。 適用場景包含：
  /// - 邏輯錯誤 (Logic Bugs): `EBADF` (操作無效 FD), `EFAULT` (非法記憶體),
  /// `EINVAL` (無效參數)
  /// - 資源耗盡 (Resource Exhaustion): `EMFILE`/`ENFILE` (FD 洩漏), `ENOMEM`
  /// (OOM), `ENOBUFS`
  /// - 環境/配置嚴重錯誤: `EADDRINUSE` (端口衝突), `EACCES` (權限不足),
  /// `ENOSPC` (磁碟滿)
  /// - 狀態機異常: 進入了 `unreachable` 的代碼分支
  ///
  /// @note 此函數會填寫 stackframe 並設定 frame_count。
  static ErrorToken capture_fail(std::error_code ec, std::string_view msg,
                                 std::source_location loc) noexcept;

  /// @brief 輕量級錯誤捕捉 - 不捕捉堆疊
  /// 僅記錄錯誤碼與發生位置，*跳過 backtrace 調用*。
  /// 成本極低，為*預期內*或*流程控制*性質錯誤設計
  ///
  /// 適用於「預期內」或「流程控制」性質的錯誤配合，例如：
  /// - 流量控制 (Flow Control): `EAGAIN`, `EWOULDBLOCK` (非阻塞 IO
  /// 暫時無數據或緩衝區滿)
  /// - 預期網路事件 (Network Noise): `EPIPE`, `ECONNRESET`
  /// (對端斷線，屬正常外部行為)
  /// - 非同步狀態 (Async State): `EINPROGRESS` (非阻塞連線握手中)
  /// - 業務邏輯拒絕 (Validation): 參數檢查失敗、無效的訂單價格等
  ///
  /// @note 此函數會將 frame_count 設為 0。
  static ErrorToken capture_flow(std::error_code ec, std::source_location loc) noexcept;
};

// ============================================================================
// 統一回傳介面
// ============================================================================

/// @brief 可能失敗調用的標準回傳類型別名
/// @note 失敗端 ErrorToken 為 1 byte，對回傳值幾乎無成本
/// @tparam T 成功的數值類型，預設為 void
template <typename T = void>
using Result = std::expected<T, ErrorToken>;

// ============================================================================
// 錯誤生成函數 (Fail & Bail)
// ============================================================================

/// @brief 將 POSIX errno 轉為標準 C++ error_code 類型
///
/// @param ev 系統 errno
/// @return 包含系統 category 的 error_code
/// @warning ec 只能為 errno, 否則沒有意義
std::error_code make_error_code(int ev) noexcept;

// ----------------------------------------------------------------------------
// Fail
// ----------------------------------------------------------------------------

/// @brief 產生錯誤結果並觸發診斷捕捉 (Fail Strategy)
/// - 完整記錄錯誤現場，內部調用 `backtrace` 保存堆疊資訊，*成本較高*
/// - 適用系統崩潰、資源耗盡、開發者邏輯錯誤 (Logic Bugs)、初始化失敗等場景
///
/// 建議使用場景：
/// - 邏輯錯誤: `EBADF` (無效FD), `EFAULT` (非法記憶體), `EINVAL`
/// (對系統調用的錯誤參數)
/// - 資源耗盡: `EMFILE`, `ENOMEM`, `ENOSPC`
/// - 嚴重環境問題: `EADDRINUSE`, `EACCES`
///
/// @param ec 錯誤代碼
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
/// @warning
/// - 僅在系統異常或不可恢復的錯誤時使用
/// - 此函式應僅在錯誤源頭呼叫
[[nodiscard]] inline auto fail(
    std::error_code ec, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  ErrorToken h = ErrorRegistry::capture_fail(ec, msg, loc);
  return std::unexpected(h);
}

/// @brief 產生錯誤結果並觸發診斷捕捉 (Fail Strategy)
/// - 完整記錄錯誤現場，內部調用 `backtrace` 保存堆疊資訊，*成本較高*
/// - 適用系統崩潰、資源耗盡、開發者邏輯錯誤 (Logic Bugs)、初始化失敗等場景
///
/// 建議使用場景：
/// - 邏輯錯誤: `EBADF` (無效FD), `EFAULT` (非法記憶體), `EINVAL`
/// (對系統調用的錯誤參數)
/// - 資源耗盡: `EMFILE`, `ENOMEM`, `ENOSPC`
/// - 嚴重環境問題: `EADDRINUSE`, `EACCES`
///
/// @param ev 系統調用返回值 (int 或 ssize_t)
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
/// @warning
/// - 僅在系統異常或不可恢復的錯誤時使用
/// - 此函式應僅在錯誤源頭呼叫
[[nodiscard]] inline auto fail(
    int ev, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = onyx::make_error_code(ev);
  ErrorToken h = ErrorRegistry::capture_fail(ec, msg, loc);
  return std::unexpected(h);
}

/// @brief 產生錯誤結果並觸發診斷捕捉 (Fail Strategy)
/// - 完整記錄錯誤現場，內部調用 `backtrace` 保存堆疊資訊，*成本較高*
/// - 適用系統崩潰、資源耗盡、開發者邏輯錯誤 (Logic Bugs)、初始化失敗等場景
///
/// 建議使用場景：
/// - 邏輯錯誤: `EBADF` (無效FD), `EFAULT` (非法記憶體), `EINVAL`
/// (對系統調用的錯誤參數)
/// - 資源耗盡: `EMFILE`, `ENOMEM`, `ENOSPC`
/// - 嚴重環境問題: `EADDRINUSE`, `EACCES`
///
/// @param ev 錯誤代碼枚舉 (std::errc)
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
/// @warning
/// - 僅在系統異常或不可恢復的錯誤時使用
/// - 此函式應僅在錯誤源頭呼叫
[[nodiscard]] inline auto fail(
    std::errc ev, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = std::make_error_code(ev);
  ErrorToken h = ErrorRegistry::capture_fail(ec, msg, loc);
  return std::unexpected(h);
}

// ----------------------------------------------------------------------------
// Bail
// ----------------------------------------------------------------------------

/// @brief 產生錯誤結果並觸發輕量級診斷捕捉 (Bail Strategy)
///
/// @param ec 錯誤代碼
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
[[nodiscard]] inline auto bail(
    std::error_code ec, std::source_location loc = std::source_location::current()) noexcept {
  ErrorToken h = ErrorRegistry::capture_flow(ec, loc);
  return std::unexpected(h);
}

/// @brief 產生錯誤結果並觸發輕量級診斷捕捉 (Bail Strategy)
/// - 僅記錄錯誤碼、訊息與位置，*不捕捉堆疊*。
/// - 適用於預期內的網路事件 (Network Noise)、流量控制 (Flow
/// Control)、參數驗證失敗
///
/// @param ev 系統調用返回值 (int 或 ssize_t)
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
[[nodiscard]] inline auto bail(
    int ev, std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = onyx::make_error_code(ev);
  ErrorToken h = ErrorRegistry::capture_flow(ec, loc);
  return std::unexpected(h);
}

/// @brief 產生錯誤結果並觸發輕量級診斷捕捉 (Bail Strategy)
/// - 僅記錄錯誤碼、訊息與位置，*不捕捉堆疊*。
/// - 適用於預期內的網路事件 (Network Noise)、流量控制 (Flow
/// Control)、參數驗證失敗
///
/// @param ev 錯誤代碼枚舉 (std::errc)
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
[[nodiscard]] inline auto bail(
    std::errc ev, std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = std::make_error_code(ev);
  ErrorToken h = ErrorRegistry::capture_flow(ec, loc);
  return std::unexpected(h);
}

}  // namespace onyx

// ============================================================================
// Macros
// ============================================================================

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
      return std::unexpected(_res.error());                         \
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

// ============================================================================
// FMT Formatters
// ============================================================================

/// @brief ErrorContext 格式化器，用來支持 fmt 格式化
/// 用來支持直接 `fmt::format("{}", err_ctx)`
///
/// 因為打印 backtrace_symbols 故鏈接器必須在編譯時期保存符號(函數名, 變數名)至
/// .dynsym，編譯選項必須添加 -rdynamic
template <>
struct fmt::formatter<onyx::ErrorContext> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  // 格式化邏輯
  template <typename FormatContext>
  auto format(const onyx::ErrorContext& ctx, FormatContext& fctx) const {
    // 基本資訊打印
    auto out = fmt::format_to(fctx.out(),
                              "\n[Error Diagnosis]\n"
                              "Message: {}\n"
                              "Code:    {} ({})\n"
                              "Source:  {}:{}\n",
                              ctx.message, ctx.ec.message(), ctx.ec.value(),
                              ctx.location.file_name(), ctx.location.line());

    // Stack Trace 處理
    if (ctx.frame_count > 0) {
      out = fmt::format_to(out, "Stack Trace:\n");

      // backtrace_symbols 內部會 malloc 一塊記憶體，存儲所有符號字串
      char** symbols = backtrace_symbols(ctx.stackframe, ctx.frame_count);
      if (symbols) {
        for (int i = 0; i < ctx.frame_count; ++i) {
          std::string sym(symbols[i]);
          std::string demangled_name = sym;

          // 嘗試解析 Linux 預設格式: ./bin(mangled_name+offset) [address]
          size_t left_paren = sym.find('(');
          size_t plus_sign = sym.find('+', left_paren);

          if (left_paren != std::string::npos && plus_sign != std::string::npos) {
            std::string mangled = sym.substr(left_paren + 1, plus_sign - left_paren - 1);

            int status = 0;
            char* res = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
            if (status == 0 && res) {
              // 重新組合更易讀的格式
              demangled_name = fmt::format("{} +{}", res, sym.substr(plus_sign + 1));
              free(res);
            }
          }
          out = fmt::format_to(out, "  #{:2d} {}\n", i, demangled_name);
        }
        free(symbols);
      } else {
        out = fmt::format_to(out, "  (Failed to capture symbols)\n");
      }
    }

    return out;
  }
};

// 讓調用端可以直接 fmt::print("Error: {}", res.error());
template <>
struct fmt::formatter<onyx::ErrorToken> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const onyx::ErrorToken& handle, FormatContext& fctx) const {
    const auto& ctx = handle.context();
    return fmt::format_to(fctx.out(), "{}", ctx);
  }
};

#endif
