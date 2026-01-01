#ifndef TX_TRADING_ENGINE_CORE_QUANTITY_HPP
#define TX_TRADING_ENGINE_CORE_QUANTITY_HPP

#include <cassert>
#include <cstdint>

namespace tx::core {

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
}  // namespace tx::core

#endif
