#ifndef ONYX_CORE_TYPE_HPP
#define ONYX_CORE_TYPE_HPP

#include <fmt/core.h>

#include <cassert>
#include <compare>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace onyx::core {

/**
 * CRTP (Curiously Recurring Template Pattern)
 * 為了解決繼承父類型不知道子類型的樣態，所以父類型無法透過 this 指針直接
 * 調用子類型方法，故才會有 vtable 來輔助。但是 vtable 會有開銷，想在編譯
 * 期間消除，就只能讓父類型知道子類型的形狀，因為 Template 會為每一個模板
 * 參數都編譯一次，所以可以將子類類型傳給父類型，父類型就知道子類型狀可以
 * 直接調用子類方法。
 */

/// @brief 基於 CRTP 的強型別定點數基底類別
///
/// @tparam Derived 繼承此類別的子類別 (本身作為 Tag 使用)
/// @tparam Scalar 縮放因子
template <typename Derived, int64_t Scalar>
class FixedPoint {
 protected:
  int64_t scaled_value_{0};

  constexpr FixedPoint() noexcept = default;
  explicit FixedPoint(int64_t val) noexcept : scaled_value_(val) {}

 public:
  // ----------------------------------------------------------------------------
  // Constants
  // ----------------------------------------------------------------------------

  inline static constexpr int64_t SCALAR = Scalar;
  inline static constexpr double SCALAR_D = static_cast<double>(Scalar);

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------
  [[nodiscard]] constexpr double to_double() const noexcept {
    return static_cast<double>(scaled_value_) / SCALAR_D;
  }

  [[nodiscard]] constexpr int64_t raw() const noexcept { return scaled_value_; }

  // ----------------------------------------------------------------------------
  // Arithmetic operations (回傳 Derived 類型: 需要子類實作 from_scaled 方法)
  // ----------------------------------------------------------------------------
  [[nodiscard]] constexpr Derived operator+(const Derived& other) const noexcept {
    assert((other.scaled_value_ > 0 && scaled_value_ <= INT64_MAX - other.scaled_value_) ||
           (other.scaled_value_ <= 0 && scaled_value_ >= INT64_MIN - other.scaled_value_));
    return Derived(scaled_value_ + other.scaled_value_);
  }

  [[nodiscard]] constexpr Derived operator-(const Derived& other) const noexcept {
    assert((other.scaled_value_ < 0 && scaled_value_ <= INT64_MAX + other.scaled_value_) ||
           (other.scaled_value_ >= 0 && scaled_value_ >= INT64_MIN + other.scaled_value_));
    return Derived(scaled_value_ - other.scaled_value_);
  }

  [[nodiscard]] constexpr Derived operator*(int64_t multiplier) const noexcept {
    assert(multiplier == 0 ||
           (scaled_value_ >= 0 && multiplier > 0 && scaled_value_ <= INT64_MAX / multiplier) ||
           (scaled_value_ >= 0 && multiplier < 0 && scaled_value_ >= INT64_MIN / multiplier) ||
           (scaled_value_ < 0 && multiplier > 0 && scaled_value_ >= INT64_MIN / multiplier) ||
           (scaled_value_ < 0 && multiplier < 0 && scaled_value_ >= INT64_MAX / multiplier));
    return Derived(scaled_value_ * multiplier);
  }

  [[nodiscard]] constexpr Derived operator/(int64_t divisor) const noexcept {
    assert(divisor != 0);
    return Derived(scaled_value_ / divisor);
  }

  constexpr Derived& operator+=(const Derived& rhs) noexcept {
    *this = *this + rhs;  // 重用 operator+ 的 assert 邏輯
    return static_cast<Derived&>(*this);
  }

  constexpr Derived& operator-=(const Derived& rhs) noexcept {
    *this = *this - rhs;  // 重用 operator- 的 assert 邏輯
    return static_cast<Derived&>(*this);
  }

  // ----------------------------------------------------------------------------
  // Comparison
  // ----------------------------------------------------------------------------
  [[nodiscard]] constexpr bool operator==(const Derived& other) const noexcept {
    return scaled_value_ == other.scaled_value_;
  }

  [[nodiscard]] constexpr std::strong_ordering operator<=>(const Derived& other) const noexcept {
    return scaled_value_ <=> other.scaled_value_;
  }

  // ----------------------------------------------------------------------------
  // Special values
  // ----------------------------------------------------------------------------

  static constexpr Derived zero() noexcept { return Derived{}; }
  static constexpr Derived max() noexcept { return Derived{std::numeric_limits<int64_t>::max()}; }
  static constexpr Derived min() noexcept { return Derived{std::numeric_limits<int64_t>::min()}; }

  // ----------------------------------------------------------------------------
  // Status
  // ----------------------------------------------------------------------------
  [[nodiscard]] constexpr bool is_positive() const noexcept { return scaled_value_ > 0; }
  [[nodiscard]] constexpr bool is_negative() const noexcept { return scaled_value_ < 0; }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return scaled_value_ == 0; }
};

/// @brief 強類型價格型別，使用定點數表示
///
/// 設計目標：
/// 1. 避免 double 的精度遺失問題。
/// 2. 支援台灣市場所有商品的最小跳動 (0.01)。
/// 3. 提供 4 位小數精度 (SCALAR = 10000)，以支援均價計算與未來擴充。
class Price : public FixedPoint<Price, 10000> {
 private:
  friend class FixedPoint;  ///< CRTP 允許存取其私有成員

  explicit constexpr Price(int64_t scaled_value) noexcept : FixedPoint(scaled_value) {}

 public:
  // ----------------------------------------------------------------------------
  // Constructor & Factory Methods
  // ----------------------------------------------------------------------------
  constexpr Price() noexcept = default;

  /// @brief 從浮點數建立
  static constexpr Price from(double val) noexcept {
    // 加上 0.5 做四捨五入，避免 100.01 變成 100.00999... 而被截斷
    int64_t raw = static_cast<int64_t>((val * SCALAR_D) + (val >= 0 ? 0.5 : -0.5));
    return Price{raw};
  }

  /// @brief 從整數值與小數位數建立
  ///
  /// @param val 整數值
  /// @param decimal_places 小數點位數
  static constexpr Price from(int64_t val, uint8_t decimal_places) noexcept {
    assert(decimal_places <= 4 && "decimal_places exceeds Price precision");

    static constexpr uint32_t scale_table[] = {1, 10, 100, 1000, 10000};

    int64_t raw = val * (SCALAR / scale_table[decimal_places]);
    return Price{raw};
  }

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  /// @brief 取得 Price 的數值
  ///
  /// @return 價格數值以符點數表示
  [[nodiscard]] constexpr double value() const noexcept { return to_double(); }

  // ----------------------------------------------------------------------------
  // Serialization
  // ----------------------------------------------------------------------------

  std::string to_string() const { return fmt::format("Price({})", to_double()); }
};

class Quantity : public FixedPoint<Quantity, 1> {
 private:
  explicit constexpr Quantity(int64_t value) noexcept : FixedPoint(value) {}

 protected:
 public:
  // ----------------------------------------------------------------------------
  // Constructors & Factory Methods
  // ----------------------------------------------------------------------------

  constexpr Quantity() noexcept = default;

  static constexpr Quantity from(int64_t value) noexcept { return Quantity{value}; }

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  /// @brief 數量數值
  ///
  /// @return 數量數值以整數表示
  [[nodiscard]] constexpr int64_t value() const noexcept { return raw(); }

  // ----------------------------------------------------------------------------
  // Serialization
  // ----------------------------------------------------------------------------

  std::string to_string() const { return fmt::format("Quantity({})", value()); }
};

/// @brief 檔位價量
struct PriceLevel {
  Price price;
  Quantity size;
};

// class OrderId {
//  private:
//   uint64_t value_;
//
//   explicit constexpr OrderId(uint64_t value) noexcept : value_(value) {}
//
//  public:
//   // ======================
//   // Named Constructors
//   // ======================
//   static constexpr OrderId from_value(uint64_t value) noexcept { return OrderId{value}; }
//
//   // ======================
//   // 特殊值
//   // ======================
//   static constexpr OrderId invalid() noexcept { return OrderId{0}; }
//
//   // ======================
//   // 取值函數
//   // ======================
//   [[nodiscard]] constexpr uint64_t value() const noexcept { return value_; }
//
//   // ======================
//   // 比較函數
//   // ======================
//   [[nodiscard]] constexpr bool operator==(const OrderId& other) const noexcept {
//     return value_ == other.value_;
//   }
//
//   [[nodiscard]] constexpr bool operator!=(const OrderId& other) const noexcept {
//     return value_ != other.value_;
//   }
//
//   // ======================
//   // 檢查函數
//   // ======================
//   [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != 0; }
// };
//
// /// @brief 交易方向
// enum class Side : uint8_t { Buy = 0, Sell = 1 };
//
// inline constexpr Side opposite(Side s) noexcept {
//   return (s == Side::Buy) ? Side::Sell : Side::Buy;
// }
//
// inline constexpr const char* to_string(Side s) noexcept {
//   switch (s) {
//     case Side::Buy:
//       return "Buy";
//     case Side::Sell:
//       return "Sell";
//   }
//   return "Unknown";
// }
//
// inline constexpr std::optional<Side> from_string(const std::string_view s) {
//   // 這邊為了效率這樣寫
//   if (s == "Buy" || s == "buy" || s == "BUY") return Side::Buy;
//   if (s == "Sell" || s == "sell" || s == "SELL") return Side::Sell;
//   return std::nullopt;
// }

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

// template <>
// struct std::hash<onyx::core::OrderId> {
//   size_t operator()(const onyx::core::OrderId& id) const noexcept {
//     // std::hash 是 functor
//     return std::hash<uint64_t>{}(id.value());
//   }
// };

#endif
