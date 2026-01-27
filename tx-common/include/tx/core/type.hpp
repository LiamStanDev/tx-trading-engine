#ifndef TX_TRADING_ENGINE_CORE_TYPE_HPP
#define TX_TRADING_ENGINE_CORE_TYPE_HPP

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

namespace tx::core {

// @brief 強類型價格型別，使用定點數表示
//
// 內部以 ticks 為單位 1 ticks = 0.01 points (台指期最小跳動)
//
class Price {
 private:
  int64_t ticks_;

  static constexpr int64_t TICK_PER_POINT = 100;  // 1 point = 100 ticks

  explicit constexpr Price(int64_t ticks) noexcept : ticks_(ticks) {}

 public:
  // ======================
  // Named Constructors
  // ======================
  static constexpr Price from_points(double points) noexcept {
    return Price{static_cast<int64_t>(points * TICK_PER_POINT)};
  }

  static constexpr Price from_ticks(int64_t ticks) noexcept {
    return Price{ticks};
  }

  // ======================
  // 取值函數
  // ======================
  [[nodiscard]] constexpr double to_points() const noexcept {
    return static_cast<double>(ticks_) / TICK_PER_POINT;
  }

  [[nodiscard]] constexpr int64_t to_ticks() const noexcept { return ticks_; }

  // ======================
  // 算術運算
  // ======================
  // brief 價格調整
  [[nodiscard]] constexpr Price operator+(const Price& other) const noexcept {
    assert(is_valid() && other.is_valid());
    assert((other.ticks_ > 0 && ticks_ <= INT64_MAX - other.ticks_) ||
           (other.ticks_ <= 0 && ticks_ >= INT64_MIN - other.ticks_));

    return Price{ticks_ + other.ticks_};
  }

  // @brief 價差運算
  [[nodiscard]] constexpr Price operator-(const Price& other) const noexcept {
    assert(is_valid() && other.is_valid());
    assert((other.ticks_ < 0 && ticks_ <= INT64_MAX + other.ticks_) ||
           (other.ticks_ >= 0 && ticks_ >= INT64_MIN + other.ticks_));
    return Price{ticks_ - other.ticks_};
  }

  // @brief 價格放大
  [[nodiscard]] constexpr Price operator*(int64_t scalar) const noexcept {
    assert(is_valid());
    assert(scalar == 0 ||
           (ticks_ >= 0 && scalar > 0 && ticks_ <= INT64_MAX / scalar) ||
           (ticks_ >= 0 && scalar < 0 && ticks_ >= INT64_MIN / scalar) ||
           (ticks_ < 0 && scalar > 0 && ticks_ >= INT64_MIN / scalar) ||
           (ticks_ < 0 && scalar < 0 && ticks_ >= INT64_MAX / scalar));

    return Price{ticks_ * scalar};
  }

  // @brief 價格縮小，會截斷 (因為整數除法)
  [[nodiscard]] constexpr Price divide_truncated(
      int64_t divisor) const noexcept {
    assert(divisor != 0);
    return Price{ticks_ / divisor};
  }

  // ======================
  // 比較運算
  // ======================
  [[nodiscard]] constexpr bool operator==(const Price& other) const noexcept {
    return ticks_ == other.ticks_;
  }

  [[nodiscard]] constexpr bool operator!=(const Price& other) const noexcept {
    return ticks_ != other.ticks_;
  }

  [[nodiscard]] constexpr bool operator<(const Price& other) const noexcept {
    return ticks_ < other.ticks_;
  }

  [[nodiscard]] constexpr bool operator<=(const Price& other) const noexcept {
    return ticks_ <= other.ticks_;
  }

  [[nodiscard]] constexpr bool operator>(const Price& other) const noexcept {
    return ticks_ > other.ticks_;
  }

  [[nodiscard]] constexpr bool operator>=(const Price& other) const noexcept {
    return ticks_ >= other.ticks_;
  }

  // ======================
  // 特殊值
  // ======================
  static constexpr Price zero() noexcept { return Price{0}; }
  static constexpr Price invalid() noexcept {
    return Price{INT64_MIN};  // 用特殊值表示無效價格
  }
  static constexpr Price max() noexcept { return Price{INT64_MAX}; }
  static constexpr Price min() noexcept { return Price{INT64_MIN + 1}; }

  [[nodiscard]] constexpr bool is_valid() const noexcept {
    return ticks_ != INT64_MIN;
  }

  // ======================
  // 序列化
  // ======================
  std::string to_string() const;
};

class Quantity {
 private:
  int64_t value_;

  explicit constexpr Quantity(int64_t value) noexcept : value_(value) {
    assert(value >= 0);
  }

 public:
  // ======================
  // Named Constructors
  // ======================
  static constexpr Quantity from_value(int64_t value) noexcept {
    return Quantity{value};
  }

  // ======================
  // 檢查函數
  // ======================
  [[nodiscard]] constexpr bool is_zero() const noexcept { return value_ == 0; }

  [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ >= 0; }

  // ======================
  // 取值函數
  // ======================
  [[nodiscard]] constexpr int64_t value() const noexcept { return value_; }

  // ======================
  // 算術運算
  // ======================
  [[nodiscard]] constexpr Quantity operator+(
      const Quantity& other) const noexcept {
    assert(value_ <= INT64_MAX - other.value_);
    return Quantity{value_ + other.value_};
  }
  [[nodiscard]] constexpr Quantity operator-(
      const Quantity& other) const noexcept {
    assert(value_ >= other.value_);
    return Quantity{value_ - other.value_};
  }

  [[nodiscard]] constexpr Quantity operator*(
      const int64_t scalar) const noexcept {
    assert(scalar >= 0);
    assert(scalar == 0 || value_ <= INT64_MAX / scalar);
    return Quantity{value_ * scalar};
  }

  [[nodiscard]] constexpr Quantity divide_exact(
      int64_t divisor) const noexcept {
    assert(divisor != 0);
    assert(value_ % divisor == 0);  // 必須整除
    return Quantity{value_ / divisor};
  }

  // ======================
  // 比較運算
  // ======================
  [[nodiscard]] constexpr bool operator==(
      const Quantity& other) const noexcept {
    return value_ == other.value_;
  }

  [[nodiscard]] constexpr bool operator!=(
      const Quantity& other) const noexcept {
    return value_ != other.value_;
  }

  [[nodiscard]] constexpr bool operator<(const Quantity& other) const noexcept {
    return value_ < other.value_;
  }

  [[nodiscard]] constexpr bool operator<=(
      const Quantity& other) const noexcept {
    return value_ <= other.value_;
  }

  [[nodiscard]] constexpr bool operator>(const Quantity& other) const noexcept {
    return value_ > other.value_;
  }

  [[nodiscard]] constexpr bool operator>=(
      const Quantity& other) const noexcept {
    return value_ >= other.value_;
  }

  // ======================
  // 特殊值
  // ======================
  static constexpr Quantity zero() noexcept { return Quantity{0}; }
  static constexpr Quantity max() noexcept { return Quantity{INT64_MAX}; }
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
  static constexpr OrderId from_value(uint64_t value) noexcept {
    return OrderId{value};
  }

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

}  // namespace tx::core

// ======================
// std::hash 特化，用來支援 unordered_map/unordered_set
// ======================
template <>
struct std::hash<tx::core::OrderId> {
  size_t operator()(const tx::core::OrderId& id) const noexcept {
    // std::hash 是 functor
    return std::hash<uint64_t>{}(id.value());
  }
};

#endif
