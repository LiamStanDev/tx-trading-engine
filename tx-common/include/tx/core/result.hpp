#ifndef TX_TRADING_ENGINE_CORE_RESULT_HPP
#define TX_TRADING_ENGINE_CORE_RESULT_HPP

#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace tx::core {

// ===========================
// Result Concept
// ===========================
template <typename T, typename E>
concept ValidResultType = (std::is_void_v<T> || std::move_constructible<T>) &&
                          std::move_constructible<E>;

template <typename R>
concept IsResult = requires {
  typename R::value_type;
  typename R::error_type;
};

// ===========================
// 正確值與錯誤包裝
// ===========================
/// @brief 成功值包裝
/// @details 用於消除 Result 構造的歧異
template <typename T>
struct Ok {
  T value;

  template <typename U>
    requires std::convertible_to<U, T>
  explicit Ok(U&& v) noexcept : value(std::forward<U>(v)) {}
};

/// @brief 錯誤值包裝
/// @details 用於消除 Result 構造的歧異
template <typename E>
struct Err {
  E error;

  template <typename U>
    requires std::convertible_to<U, E>
  explicit Err(U&& e) noexcept : error(std::forward<U>(e)) {}
};

/// @brief void 特化
template <>
struct Ok<void> {};

// ===========================
// Deduction guide
// ===========================
template <typename T = void>
Ok(T) -> Ok<T>;

template <typename E>
Err(E) -> Err<E>;

/// @brief 無異常的錯誤處理
/// @tparam T 成功類型
/// @tparam E 錯誤類型
/// @details
///  - Release 模式下零開銷抽象
///  - Debug 模式下 assert 進行安全檢查
///  - 透過 [[nodiscard]] 強制錯誤檢查
template <typename T, typename E>
class [[nodiscard]] Result {
 private:
  std::variant<T, E> data_;

  /// @brief 統一構造
  /// @details 這樣只要做一次 Concept 約事就好
  template <size_t Index, typename Value>
  Result(std::in_place_index_t<Index>, Value&& value) noexcept
    requires ValidResultType<T, E>
      : data_(std::in_place_index<Index>, std::forward<Value>(value)) {}

 public:
  using value_type = T;
  using error_type = E;

  // ===========================
  // 構造函數
  // ===========================
  /// @brief 建構成功值
  /// @details 採用委託構造
  Result(Ok<T> ok) noexcept  // NOLINT
      : Result(std::in_place_index<0>, std::move(ok.value)) {}
  /// @brief 建構錯誤值
  /// @details 採用委託構造
  Result(Err<E> err) noexcept  // NOLINT
      : Result(std::in_place_index<1>, std::move(err.error)) {}

  // ===========================
  // 狀態查詢
  // ===========================
  [[nodiscard]] bool is_ok() const noexcept { return data_.index() == 0; }
  [[nodiscard]] bool is_err() const noexcept { return data_.index() == 1; }
  [[nodiscard]] explicit operator bool() const noexcept { return is_ok(); };

  // ===========================
  // 訪問
  // ===========================
  // NOTE:  std::get<> 支持移動語意
  [[nodiscard]] T& operator*() & noexcept {
    // 這會拋異常 std::bad_variant_access
    // return std::get<0>(data_);

    assert(is_ok() && "Dereference Result in Err state");
    return *std::get_if<0>(&data_);
  }
  [[nodiscard]] const T& operator*() const& noexcept {
    assert(is_ok() && "Dereference Result in Err state");
    return *std::get_if<0>(&data_);
  }
  [[nodiscard]] T&& operator*() && noexcept {
    assert(is_ok() && "Dereference Result in Err state");
    return std::move(*std::get_if<0>(&data_));
  }
  [[nodiscard]] const T&& operator*() const&& noexcept {
    assert(is_ok() && "Dereference Result in Err state");
    return std::move(*std::get_if<0>(&data_));
  }

  [[nodiscard]] T* operator->() noexcept {
    assert(is_ok() && "Accessed member of Result in Err state");
    return std::get_if<0>(&data_);
  }
  [[nodiscard]] const T* operator->() const noexcept {
    assert(is_ok() && "Accessed member of Result in Err state");
    return std::get_if<0>(&data_);
  };

  [[nodiscard]] T& value() & noexcept { return *(*this); }
  [[nodiscard]] const T& value() const& noexcept { return *(*this); }
  [[nodiscard]] T&& value() && noexcept { return *std::move(*this); }
  [[nodiscard]] const T&& value() const&& noexcept { return *std::move(*this); }

  [[nodiscard]] E& error() & noexcept {
    assert(is_err() && "Called error() on Ok");
    return *std::get_if<1>(&data_);
  }
  [[nodiscard]] const E& error() const& noexcept {
    assert(is_err() && "Called error() on Ok");
    return *std::get_if<1>(&data_);
  }
  [[nodiscard]] E&& error() && noexcept {
    assert(is_err() && "Called error() on Ok");
    return std::move(*std::get_if<1>(&data_));
  }
  [[nodiscard]] const E&& error() const&& noexcept {
    assert(is_err() && "Called error() on Ok");
    return std::move(*std::get_if<1>(&data_));
  }

  // ===========================
  // 消耗性方法
  // ===========================
  /// @brief 解包成功值，錯誤時 assert（Debug）或 UB（Release）
  [[nodiscard]] T unwrap() && noexcept {
    assert(is_ok() && "Called unwrap() on Err");
    return std::move(*this).value();
  }
  /// @brief 解包成功值，或返回默認值
  [[nodiscard]] T unwrap_or(T default_value) && noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    return is_ok() ? std::move(*this).value() : std::move(default_value);
  }
  /// @brief 解包成功值，或調用函數計算默認值
  /// @tparam Fn 函數類型，簽名為 T(E)
  /// @details 延遲計算：只在 Err 時調用 fn
  template <typename Fn>
    requires std::invocable<Fn, E> &&
             std::convertible_to<std::invoke_result_t<Fn, E>, T>
  [[nodiscard]] T unwrap_or_else(Fn fn) && noexcept(
      noexcept(fn(std::declval<E>()))) {
    return is_ok() ? std::move(*this).value() : fn(std::move(*this).error());
  }

  // ===========================
  // 函數式
  // ===========================
  // @brief 對成功值應用函數，保持 Err 不變
  /// @tparam Fn 函數類型，簽名為 U(T)
  /// @return Result<U, E>
  /// @details map: Fn(T) -> U
  template <typename Fn>
    requires std::invocable<Fn, T> &&
             std::move_constructible<std::invoke_result_t<Fn, T>>
  [[nodiscard]] auto map(Fn fn) && -> Result<std::invoke_result_t<Fn, T>, E> {
    using U = std::invoke_result_t<Fn, T>;

    if (is_ok()) [[likely]] {
      return Ok<U>(fn(std::move(*this).value()));
    }
    return Err<E>(std::move(*this).error());
  }

  /// @brief 鏈式調用（flatMap）
  /// @tparam Fn 函數類型，簽名為 Result<U, E>(T)
  /// @return Result<U, E>
  /// @details map: Fn(T) -> Result<U, E>
  template <typename Fn>
  [[nodiscard]] auto and_then(Fn fn) && -> std::invoke_result_t<Fn, T>
    requires std::invocable<Fn, T> && IsResult<std::invoke_result_t<Fn, T>> &&
             std::same_as<typename std::invoke_result_t<Fn, T>::error_type, E>
  {
    if (is_ok()) [[likely]] {
      return fn(std::move(*this).value());
    }
    return Err<E>(std::move(*this).error());
  }

  // @brief 對錯誤值應用函數，保持 T 不變
  /// @tparam Fn 函數類型，簽名為 U(E)
  /// @return Result<T, U>
  /// @details map: Fn(E) -> U
  template <typename Fn>
  [[nodiscard]] auto map_err(Fn fn) && -> Result<T, std::invoke_result_t<Fn, E>>
    requires std::invocable<Fn, E> &&
             std::move_constructible<std::invoke_result_t<Fn, E>>

  {
    using F = std::invoke_result_t<Fn, E>;

    if (is_err()) [[unlikely]] {
      return Err<F>(fn(std::move(*this).error()));
    }
    return Ok<T>(std::move(*this).value());
  }
};

/// @brief Result<void, E> 特化：用於無返回值的操作
template <typename E>
class [[nodiscard]] Result<void, E> {
 private:
  std::variant<std::monostate, E> data_;

 public:
  using value_type = void;
  using error_type = E;

  // ===========================
  // 構造函數
  // ===========================
  Result(Ok<void>) noexcept  // NOLINT
    requires ValidResultType<void, E>
      : data_(std::in_place_index<0>) {}
  Result(Err<E> err) noexcept  // NOLINT
    requires ValidResultType<void, E>
      : data_(std::in_place_index<1>, std::move(err.error)) {}

  // ===========================
  // 狀態查詢
  // ===========================
  [[nodiscard]] constexpr bool is_ok() const noexcept {
    return data_.index() == 0;
  }
  [[nodiscard]] constexpr bool is_err() const noexcept {
    return data_.index() == 1;
  }
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return is_ok();
  }

  // ===========================
  // 訪問
  // ===========================
  [[nodiscard]] E& error() & noexcept {
    assert(is_err() && "Called error() on Ok");
    return std::get<1>(data_);
  }
  [[nodiscard]] const E& error() const& noexcept {
    assert(is_err() && "Called error() on Ok");
    return std::get<1>(data_);
  }
  [[nodiscard]] E&& error() && noexcept {
    assert(is_err() && "Called error() on Ok");
    return std::get<1>(std::move(data_));
  }
  [[nodiscard]] const E&& error() const&& noexcept {
    assert(is_err() && "Called error() on Ok");
    return std::get<1>(std::move(data_));
  }

  void unwrap() && noexcept { assert(is_ok() && "Called unwrap() on Err"); }

  // ===========================
  // 函數式
  // ===========================
  // @brief 對錯誤值應用函數，保持 T 不變
  /// @tparam Fn 函數類型，簽名為 U(E)
  /// @return Result<void, U>
  /// @details map: Fn(E) -> U
  template <typename Fn>
  [[nodiscard]] auto map_err(
      Fn fn) && -> Result<void, std::invoke_result_t<Fn, E>>
    requires std::invocable<Fn, E> &&
             std::move_constructible<std::invoke_result_t<Fn, E>>

  {
    using F = std::invoke_result_t<Fn, E>;

    if (is_err()) [[unlikely]] {
      return Err<F>(fn(std::move(*this).error()));
    }
    return Ok<void>();
  }
};

}  // namespace tx::core

namespace tx {

template <typename T = void>
using Ok = core::Ok<T>;

template <typename E>
using Err = core::Err<E>;

template <typename T, typename E>
using Result = core::Result<T, E>;

}  // namespace tx

/// @brief 解包 Result<T, E>，失敗時提前返回
/// @details 使用 GNU Statement Expression 提供類似 Rust ? operator 的語法
/// @warning 只能在返回 Result<T, E> 的函數中使用
///
/// @example
///   auto socket = TRY(Socket::create_tcp());
///   auto addr = TRY(SocketAddress::from_ipv4("127.0.0.1", 8080));
#define TRY(expr)                                                  \
  ({                                                               \
    auto&& _try_result = (expr);                                   \
    using _TryResult = std::decay_t<decltype(_try_result)>;        \
    using _TryError = typename _TryResult::error_type;             \
                                                                   \
    if (!_try_result) [[unlikely]] {                               \
      return ::tx::Err<_TryError>(std::move(_try_result).error()); \
    }                                                              \
    std::move(_try_result).unwrap();                               \
  })

/// @brief 檢查 Result<void, E>，失敗時提前返回
/// @details 用於不需要返回值的場景（如 Result<void, E>）
/// @warning 只能在返回 Result<T, E> 的函數中使用
///
/// @example
///   CHECK(socket.connect(addr));
///   CHECK(socket.set_nonblocking(true));
#define CHECK(expr)                                                    \
  do {                                                                 \
    auto&& _check_result = (expr);                                     \
    using _CheckResult = std::decay_t<decltype(_check_result)>;        \
    using _CheckError = typename _CheckResult::error_type;             \
                                                                       \
    if (!_check_result) [[unlikely]] {                                 \
      return ::tx::Err<_CheckError>(std::move(_check_result).error()); \
    }                                                                  \
    std::move(_check_result).unwrap();                                 \
  } while (0)

#endif  // TX_TRADING_ENGINE_CORE_RESULT_HPP
