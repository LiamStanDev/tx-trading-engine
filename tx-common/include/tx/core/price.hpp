#ifndef TX_TRADING_ENGINE_CORE_PRICE_HPP
#define TX_TRADING_ENGINE_CORE_PRICE_HPP

#include <cassert>
#include <cstdint>
#include <string>

namespace tx::core {

// @brief 強類型價格型別，使用定點數表示
// @defails 內部以 ticks 為單位 1 ticks = 0.01 points (台指期最小跳動)
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
}  // namespace tx::core

#endif
