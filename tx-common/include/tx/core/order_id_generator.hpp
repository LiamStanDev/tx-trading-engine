#ifndef TX_TRADING_ENGINE_CORE_ORDER_ID_GENERATOR_HPP
#define TX_TRADING_ENGINE_CORE_ORDER_ID_GENERATOR_HPP

#include <atomic>
#include <cstdint>

#include "tx/core/order_id.hpp"

namespace tx::core {

class OrderIdGenerator {
 private:
  // Cache-line 對齊，避免 False Sharing
  // 如果有兩個 OrderIdGenerator 靠在一起，沒有對齊會導致 cache 相互影響
  alignas(64) std::atomic<uint64_t> counter_{1};

 public:
  /// @brief 生成下一個唯一的 OrderId
  /// @return 新的 OrderId
  /// @note 執行續安全
  OrderId next() noexcept {
    uint64_t value = counter_.fetch_add(1, std::memory_order_relaxed);
    return OrderId::from_value(value);
  }

  /// @brief 重製計數器
  /// @param start 起始值 (預設為 1)
  /// @warning 非執行續安全，如果同時在呼叫 next() 可能產生混亂
  void reset(uint64_t start = 1) noexcept {
    counter_.store(start, std::memory_order_relaxed);
  }

  /// @brief 取得當前計數器值（不消耗 ID）
  /// @return 當前計數器值
  /// @note 僅供除錯使用，多執行緒下可能立即過期
  uint64_t current() const noexcept {
    return counter_.load(std::memory_order_relaxed);
  }
};

}  // namespace tx::core

#endif
