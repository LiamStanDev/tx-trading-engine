#ifndef ONYX_ERROR_HPP
#define ONYX_ERROR_HPP

#include <cxxabi.h>
#include <fmt/format.h>
#include <stdint.h>

#include <array>
#include <expected>
#include <source_location>
#include <system_error>

#include "execinfo.h"

namespace onyx {

/// @brief 錯誤發生上下文資訊
struct alignas(64) ErrorContext {
  static inline constexpr size_t MAX_FRAME_COUNT = 6;

  void* stackframe[MAX_FRAME_COUNT];  // 保存調用棧位置
  std::error_code ec;                 ///< 發生時原始錯誤碼
  const char* message;                ///< 上下文說明
  std::source_location location;      ///< 發生時錯誤位置
  uint8_t frame_count;                // 實際使用保存多少個調用棧

  friend bool operator==(const ErrorContext& lhs, const std::error_code& rhs) noexcept {
    return lhs.ec == rhs;
  }

  friend bool operator==(const ErrorContext& lhs, const std::errc& rhs) noexcept {
    return lhs.ec == rhs;
  }
};

static_assert(sizeof(ErrorContext) <= 64 * 2, "Size of ErrorContext should less then 2 cachelines");

/// @brief 錯誤句柄，可用於查詢錯誤資訊
/// 用於 std::expected<T, ErrorHandle> 異常返回值
struct ErrorHandle {
  uint32_t id;

  constexpr bool operator==(const ErrorHandle& other) const noexcept { return id == other.id; }

  const ErrorContext& context() const noexcept;
  std::error_code ec() const noexcept;
  std::string_view message() const noexcept;

  friend bool operator==(const ErrorHandle& lhs, const std::error_code& rhs) noexcept {
    return lhs.context().ec == rhs;
  }

  friend bool operator==(const ErrorHandle& lhs, const std::errc& rhs) noexcept {
    return lhs.context().ec == rhs;
  }
};

static_assert(sizeof(ErrorHandle) <= 4, "Size of ErrorHandle should less then 4 bytes");

/// @brief 執行緒局部的錯誤診斷暫存器
/// 用來保存 ErrorContext 內容，不透過 `std::exepcted` 的 `E`
/// 返回大量數據，降低返回值成本
///
/// @note 每個執行緒獨立管理其最後一次錯誤路徑。
class ErrorRegistry {
 public:
  // NOTE: 這邊使用 u32 不是 size_t 是因為我不希望 ErrorHandle 太大
  static inline constexpr uint32_t SIZE = 16;         ///< 錯誤 RingBuffer 大小
  static inline constexpr size_t MSG_MAX_LEN = 1023;  //< 錯誤訊息長度 (保留 1 byte 給 '\0')

 private:
  static inline thread_local std::array<ErrorContext, SIZE> ctx_buffer_{};  ///< Error RingBuffer
  static inline thread_local std::array<std::array<char, MSG_MAX_LEN + 1>, SIZE> msg_buffer_{};
  static inline thread_local uint32_t head_{0};  ///< 獲取當前執行緒最新錯誤指標

 public:
  /// @brief 錯誤捕捉 - 含完整調用棧
  /// 完整記錄錯誤現場，內部調用 `backtrace` 保存堆疊資訊，*成本較高*，專門為
  /// 適用於*非預期*或*系統/邏輯崩潰*性質錯誤設計
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
  static ErrorHandle capture_origin(std::error_code ec, std::string_view msg,
                                    std::source_location loc) noexcept;

  /// @brief 輕量級錯誤捕捉 - 不捕捉堆疊
  /// 僅記錄錯誤碼、靜態訊息與發生位置，*跳過 backtrace 調用*。
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
  static ErrorHandle capture_origin_thin(std::error_code ec, std::string_view msg,
                                         std::source_location loc) noexcept;

  /// @brief 獲取錯誤詳情
  ///
  static const ErrorContext& get(ErrorHandle h) noexcept {
    if (head_ - h.id > SIZE) [[unlikely]] {
      return EXPIRED_CONTEXT;
    }

    return ctx_buffer_[h.id % SIZE];
  }

 private:
  static inline const ErrorContext EXPIRED_CONTEXT{
      .stackframe = {},
      .ec = std::make_error_code(std::errc::state_not_recoverable),
      .message = "<EXPIRED ERROR HANDLE>",
      .location = std::source_location{},  // 使用默認
      .frame_count = 0,
  };
};

// ============================================================================
// 統一回傳介面
// ============================================================================

/// @brief 可能失敗調用的標準回傳類型別名
/// @note ErrorHandle (4 bytes) 為失敗索引
/// @tparam T 成功的數值類型，預設為 void
template <typename T = void>
using Result = std::expected<T, ErrorHandle>;

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
  ErrorHandle h = ErrorRegistry::capture_origin(ec, msg, loc);
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
  ErrorHandle h = ErrorRegistry::capture_origin(ec, msg, loc);
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
  ErrorHandle h = ErrorRegistry::capture_origin(ec, msg, loc);
  return std::unexpected(h);
}

// ----------------------------------------------------------------------------
// Bail
// ----------------------------------------------------------------------------

/// @brief 產生錯誤結果並觸發輕量級診斷捕捉 (Bail Strategy)
/// - 僅記錄錯誤碼、訊息與位置，*不捕捉堆疊*。
/// - 適用於預期內的網路事件 (Network Noise)、流量控制 (Flow
/// Control)、參數驗證失敗
///
/// @param ec 錯誤代碼
/// @param msg 靜態描述字串，解釋錯誤背景
/// @param loc 自動捕捉呼叫處的原始碼位置
/// @return 一個封裝了錯誤碼的 std::unexpected
[[nodiscard]] inline auto bail(
    std::error_code ec, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  ErrorHandle h = ErrorRegistry::capture_origin_thin(ec, msg, loc);
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
    int ev, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = onyx::make_error_code(ev);
  ErrorHandle h = ErrorRegistry::capture_origin_thin(ec, msg, loc);
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
    std::errc ev, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  std::error_code ec = std::make_error_code(ev);
  ErrorHandle h = ErrorRegistry::capture_origin_thin(ec, msg, loc);
  return std::unexpected(h);
}

/// @brief 系統錯誤處置策略
enum class ErrorStrategy : uint8_t {
  Bail,  ///< 預期內、可恢復或外部因素 -> 輕量返回
  Fail   ///< 邏輯錯誤、資源耗盡或未知異常 -> 捕捉堆疊
};

/// @brief 根據 Linux errno 判斷處理策略
///
/// @param err 系統 errno (e.g., EAGAIN, EBADF)
/// @return ErrorStrategy::Bail 或 Fail
constexpr ErrorStrategy classify_errno(int err) noexcept {
  using enum onyx::ErrorStrategy;

  switch (err) {
    // -----------------------------------------------------------------
    // Group 1: 流量控制與非阻塞狀態 (Flow Control)
    // -----------------------------------------------------------------
    case EAGAIN:  // 資源暫時不可用 (Try again)
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:  // 操作會阻塞 (Linux 上通常等同 EAGAIN)
#endif
    case EINTR:  // 系統調用被信號中斷
      return Bail;

    // -----------------------------------------------------------------
    // Group 2: 非阻塞連線狀態 (Async Connection)
    // -----------------------------------------------------------------
    case EINPROGRESS:  // 連線正在進行中 (Non-blocking connect)
    case EALREADY:     // Socket 已經在進行連線操作
    case EISCONN:      // Socket 已經連線 (重複呼叫 connect)
      return Bail;

    // -----------------------------------------------------------------
    // Group 3: 網路生命週期與外部事件 (Network Lifecycle)
    // -----------------------------------------------------------------
    case EPIPE:         // 寫入已關閉的管道 (對端 RST)
    case ECONNRESET:    // 連線被對端重置
    case ECONNABORTED:  // 連線被軟體中止
    case ETIMEDOUT:     // 連線超時
    case EHOSTUNREACH:  // 主機不可達 (路由問題)
    case ENETUNREACH:   // 網絡不可達
    case ENETDOWN:      // 網絡介面關閉
    case ECONNREFUSED:  // 連線被拒絕 (Port 未監聽)
    case ENOTCONN:      // Socket 未連線 (發送時)
    case ESHUTDOWN:     // Socket 已經 shutdown
      return Bail;

    // -----------------------------------------------------------------
    // Group 4: 協議與請求限制 (Protocol Limits) - 視為 Client 端錯誤
    // -----------------------------------------------------------------
    case EMSGSIZE:       // 訊息太長 (UDP) -> 拒絕該次請求，不用 Crash
    case EPROTOTYPE:     // 協議類型錯誤
    case ENOPROTOOPT:    // 協議選項不可用
    case EADDRNOTAVAIL:  // 地址不可用 (可能是 bind 參數錯，也可能是 ephemeral
                         // port 耗盡) 註：這有點邊界，但在高頻發送時若 port
                         // 用光，算資源暫時不足 -> Bail
      return Bail;

    // -----------------------------------------------------------------
    // Group 5: 取消與其他輕量錯誤
    // -----------------------------------------------------------------
    case ECANCELED:  // AIO 操作被取消
    case EIDRM:      // 標識符被刪除 (IPC)
      return Bail;

    // -----------------------------------------------------------------
    // Group 6: 必須 FAIL 的嚴重錯誤 (Explicit Fail List)
    // -----------------------------------------------------------------
    case EBADF:       // 無效的文件描述符 → 程式邏輯錯誤 (Double close / Use after close)
    case EFAULT:      // 非法內存地址 → 程式邏輯錯誤 (指標亂指)
    case EINVAL:      // 無效參數 → 程式邏輯錯誤 (傳錯參數)
    case EMFILE:      // 進程 FD 耗盡(資源洩漏)
    case ENFILE:      // 系統 FD 耗盡 (嚴重環境問題)
    case ENOMEM:      // 內存不足 (OOM)
    case EACCES:      // 權限不足 (配置錯誤)
    case EADDRINUSE:  // 地址被佔用 (啟動失敗)
    case ENOTSOCK:    // 對非 Socket fd 操作 socket api

    default:
      // 防禦性編程：任何沒看過的錯誤，都視為異常，保留現場
      return Fail;
  }
}

/// @brief errno 自動分流錯誤處理 (Auto-Dispatch Strategy)
/// 根據錯誤碼的嚴重性，自動決定使用 bail (輕量) 或 fail (重量/Backtrace)
///
/// @param ev 系統調用返回值 (int 或 ssize_t)
/// @param msg 錯誤訊息
/// @param loc 捕捉位置
/// @return 總是返回 std::unexpected (代表失敗)
[[nodiscard]] inline auto wrap_errno(
    int ev, std::string_view msg = "",
    std::source_location loc = std::source_location::current()) noexcept {
  // 自動區分
  if (classify_errno(ev) == ErrorStrategy::Bail) {
    return onyx::bail(ev, msg, loc);
  } else {
    return onyx::fail(ev, msg, loc);
  }
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
    const char* msg_to_print = (ctx.message && ctx.message[0] != '\0') ? ctx.message : "None";
    // 基本資訊打印
    auto out = fmt::format_to(fctx.out(),
                              "\n[Error Diagnosis]\n"
                              "Message: {}\n"
                              "Code:    {} ({})\n"
                              "Source:  {}:{}\n",
                              msg_to_print, ctx.ec.message(), ctx.ec.value(),
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
struct fmt::formatter<onyx::ErrorHandle> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const onyx::ErrorHandle& handle, FormatContext& fctx) const {
    const auto& ctx = handle.context();
    return fmt::format_to(fctx.out(), "{}", ctx);
  }
};

#endif
