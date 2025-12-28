#ifndef TX_TRADING_ENGINE_CORE_ORDER_ID_HPP
#define TX_TRADING_ENGINE_CORE_ORDER_ID_HPP

#include <cstdint>
#include <functional>

namespace tx::core {

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
