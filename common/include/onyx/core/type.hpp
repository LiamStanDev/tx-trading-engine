#ifndef ONYX_CORE_TYPE_HPP
#define ONYX_CORE_TYPE_HPP

#include <cassert>
#include <compare>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>

namespace onyx::core {

/// @brief 強類型價格型別，使用定點數表示
///
/// 設計目標：
/// 1. 避免 double 的精度遺失問題。
/// 2. 支援台灣市場所有商品的最小跳動 (0.01)。
/// 3. 提供 4 位小數精度 (SCALAR = 10000)，以支援均價計算與未來擴充。
class Price {
 private:
  int64_t scaled_value_;  ///< 儲存值： 1 代表 0.0001 點 (依據 SCALAR)

  explicit constexpr Price(int64_t scaled_value) noexcept : scaled_value_(scaled_value) {}

 public:
  // ----------------------------------------------------------------------------
  // Constants
  // ----------------------------------------------------------------------------

  // 縮放因子：10000 (支援到小數點下 4 位)
  // 1.0 點 = 10000 units
  // 0.01 點 = 100 units
  static constexpr int64_t SCALAR = 10000;
  static constexpr double SCALAR_D = 10000.0;
  static constexpr int64_t INVALID_VAL = std::numeric_limits<int64_t>::min();

  // ----------------------------------------------------------------------------
  // Constructor & Factory Methods
  // ----------------------------------------------------------------------------
  constexpr Price() noexcept : scaled_value_(INVALID_VAL) {}

  /// @brief 從浮點數建立
  static constexpr Price from_double(double points) noexcept {
    // 加上 0.5 做四捨五入，避免 100.01 變成 100.00999... 而被截斷
    double val = (points * SCALAR_D) + (points >= 0 ? 0.5 : -0.5);
    return Price{static_cast<int64_t>(val)};
  }

  /// @brief 從原始整數建立 (Raw Ticks)
  ///
  /// @param raw_value 必須已經是乘過 SCALAR 的值
  static constexpr Price from_raw(int64_t raw_value) noexcept { return Price{raw_value}; }

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  [[nodiscard]] constexpr double to_double() const noexcept {
    return static_cast<double>(scaled_value_) / SCALAR;
  }

  /// @brief 取得底層整數
  [[nodiscard]] constexpr int64_t raw() const noexcept { return scaled_value_; }

  // ----------------------------------------------------------------------------
  // Arithmetic operations
  // ----------------------------------------------------------------------------

  // brief 價格調整
  [[nodiscard]] constexpr Price operator+(const Price& other) const noexcept {
    assert(is_valid() && other.is_valid());
    assert((other.scaled_value_ > 0 && scaled_value_ <= INT64_MAX - other.scaled_value_) ||
           (other.scaled_value_ <= 0 && scaled_value_ >= INT64_MIN - other.scaled_value_));

    return Price{scaled_value_ + other.scaled_value_};
  }

  // @brief 價差運算
  [[nodiscard]] constexpr Price operator-(const Price& other) const noexcept {
    assert(is_valid() && other.is_valid());
    assert((other.scaled_value_ < 0 && scaled_value_ <= INT64_MAX + other.scaled_value_) ||
           (other.scaled_value_ >= 0 && scaled_value_ >= INT64_MIN + other.scaled_value_));
    return Price{scaled_value_ - other.scaled_value_};
  }

  // @brief 價格放大
  [[nodiscard]] constexpr Price operator*(int64_t scalar) const noexcept {
    assert(is_valid());
    assert(scalar == 0 ||
           (scaled_value_ >= 0 && scalar > 0 && scaled_value_ <= INT64_MAX / scalar) ||
           (scaled_value_ >= 0 && scalar < 0 && scaled_value_ >= INT64_MIN / scalar) ||
           (scaled_value_ < 0 && scalar > 0 && scaled_value_ >= INT64_MIN / scalar) ||
           (scaled_value_ < 0 && scalar < 0 && scaled_value_ >= INT64_MAX / scalar));

    return Price{scaled_value_ * scalar};
  }

  // @brief 價格縮小，會截斷 (因為整數除法)
  [[nodiscard]] constexpr Price operator/(int64_t divisor) const noexcept {
    assert(is_valid() && divisor != 0);
    return Price{scaled_value_ / divisor};
  }

  constexpr Price& operator+=(const Price& rhs) noexcept {
    scaled_value_ += rhs.scaled_value_;
    return *this;
  }
  constexpr Price& operator-=(const Price& rhs) noexcept {
    scaled_value_ -= rhs.scaled_value_;
    return *this;
  }

  // ----------------------------------------------------------------------------
  // Comparison operaitons
  // ----------------------------------------------------------------------------

  [[nodiscard]] constexpr bool operator==(const Price& other) const noexcept {
    return scaled_value_ == other.scaled_value_;
  }

  [[nodiscard]] constexpr std::strong_ordering operator<=>(const Price& other) const noexcept {
    return scaled_value_ <=> other.scaled_value_;
  }

  // ----------------------------------------------------------------------------
  // Special values
  // ----------------------------------------------------------------------------

  static constexpr Price zero() noexcept { return Price{0}; }
  static constexpr Price max() noexcept { return Price{std::numeric_limits<int64_t>::max()}; }
  static constexpr Price min() noexcept { return Price{std::numeric_limits<int64_t>::min() + 1}; }
  static constexpr Price invalid() noexcept { return Price{INVALID_VAL}; }

  // ----------------------------------------------------------------------------
  // Status
  // ----------------------------------------------------------------------------

  [[nodiscard]] constexpr bool is_valid() const noexcept { return scaled_value_ != INVALID_VAL; }
  [[nodiscard]] constexpr bool is_positive() const noexcept { return scaled_value_ > 0; }
  [[nodiscard]] constexpr bool is_negative() const noexcept { return scaled_value_ < 0; }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return scaled_value_ == 0; }

  // ----------------------------------------------------------------------------
  // Serialization
  // ----------------------------------------------------------------------------

  std::string to_string() const;
};

class Quantity {
 private:
  int64_t value_;

  explicit constexpr Quantity(int64_t value) noexcept : value_(value) {
    assert(value >= 0 && "Quantity cannot be negative");
  }

 public:
  // ----------------------------------------------------------------------------
  // Constructors & Factory Methods
  // ----------------------------------------------------------------------------

  constexpr Quantity() noexcept : value_(0) {}

  static constexpr Quantity from_value(int64_t value) noexcept { return Quantity{value}; }

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  [[nodiscard]] constexpr int64_t value() const noexcept { return value_; }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return value_ == 0; }
  [[nodiscard]] constexpr bool is_positive() const noexcept { return value_ > 0; }
  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ >= 0; }

  // ----------------------------------------------------------------------------
  // Comparison operations
  // ----------------------------------------------------------------------------
  [[nodiscard]] constexpr bool operator==(const Quantity& rhs) const noexcept {
    return value_ == rhs.value_;
  }

  [[nodiscard]] constexpr std::strong_ordering operator<=>(const Quantity& other) const noexcept {
    return value_ <=> other.value_;
  }

  // ----------------------------------------------------------------------------
  //  Arithmetic operations
  // ----------------------------------------------------------------------------

  [[nodiscard]] constexpr Quantity operator+(const Quantity& other) const noexcept {
    assert(value_ <= INT64_MAX - other.value_);
    return Quantity{value_ + other.value_};
  }
  [[nodiscard]] constexpr Quantity operator-(const Quantity& other) const noexcept {
    assert(value_ >= other.value_);
    return Quantity{value_ - other.value_};
  }

  [[nodiscard]] constexpr Quantity operator*(const int64_t scalar) const noexcept {
    assert(scalar >= 0);
    assert(scalar == 0 || value_ <= INT64_MAX / scalar);
    return Quantity{value_ * scalar};
  }

  [[nodiscard]] constexpr Quantity operator/(int64_t divisor) const noexcept {
    assert(divisor != 0);
    return Quantity{value_ / divisor};
  }

  constexpr Quantity& operator+=(const Quantity& rhs) noexcept {
    value_ += rhs.value_;
    return *this;
  }
  constexpr Quantity& operator-=(const Quantity& rhs) noexcept {
    value_ -= rhs.value_;
    return *this;
  }

  // ----------------------------------------------------------------------------
  // Special values
  // ----------------------------------------------------------------------------

  static constexpr Quantity zero() noexcept { return Quantity{0}; }
  static constexpr Quantity max() noexcept { return Quantity{std::numeric_limits<int64_t>::max()}; }
  static constexpr Quantity min() noexcept { return Quantity{0}; }
};

class OrderId {
 private:
  uint64_t value_;

  explicit constexpr OrderId(uint64_t value) noexcept : value_(value) {}

 public:
  // ======================
  // Named Constructors
  // ======================
  static constexpr OrderId from_value(uint64_t value) noexcept { return OrderId{value}; }

  // ======================
  // 特殊值
  // ======================
  static constexpr OrderId invalid() noexcept { return OrderId{0}; }

  // ======================
  // 取值函數
  // ======================
  [[nodiscard]] constexpr uint64_t value() const noexcept { return value_; }

  // ======================
  // 比較函數
  // ======================
  [[nodiscard]] constexpr bool operator==(const OrderId& other) const noexcept {
    return value_ == other.value_;
  }

  [[nodiscard]] constexpr bool operator!=(const OrderId& other) const noexcept {
    return value_ != other.value_;
  }

  // ======================
  // 檢查函數
  // ======================
  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != 0; }
};

/// @brief 交易方向
enum class Side : uint8_t { Buy = 0, Sell = 1 };

inline constexpr Side opposite(Side s) noexcept {
  return (s == Side::Buy) ? Side::Sell : Side::Buy;
}

inline constexpr const char* to_string(Side s) noexcept {
  switch (s) {
    case Side::Buy:
      return "Buy";
    case Side::Sell:
      return "Sell";
  }
  return "Unknown";
}

inline constexpr std::optional<Side> from_string(const std::string_view s) {
  // 這邊為了效率這樣寫
  if (s == "Buy" || s == "buy" || s == "BUY") return Side::Buy;
  if (s == "Sell" || s == "sell" || s == "SELL") return Side::Sell;
  return std::nullopt;
}

}  // namespace onyx::core

// ----------------------------------------------------------------------------
//  Hash support
// ----------------------------------------------------------------------------

template <>
struct std::hash<onyx::core::Price> {
  size_t operator()(const onyx::core::Price& p) const noexcept {
    return std::hash<int64_t>{}(p.raw());
  }
};

template <>
struct std::hash<onyx::core::OrderId> {
  size_t operator()(const onyx::core::OrderId& id) const noexcept {
    // std::hash 是 functor
    return std::hash<uint64_t>{}(id.value());
  }
};

#endif
