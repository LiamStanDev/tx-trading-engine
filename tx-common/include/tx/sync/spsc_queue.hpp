/// @file spsc_queue.hpp
/// @brief 單消費者單生產者環形佇列
///
/// @author Liam Lin
/// @date 2026-01-23
///

#ifndef TX_TRADING_ENGINE_SYNC_SPSC_QUEUE_HPP
#define TX_TRADING_ENGINE_SYNC_SPSC_QUEUE_HPP

#include <array>
#include <atomic>
#include <concepts>
#include <optional>

namespace tx::sync {

/// @brief 無鎖單消費者單生產者環形佇列
///
/// 用於兩個執行續之間進行資料交互使用的高速管道, 採用無鎖設計
/// 以做到高吞吐低延遲。
/// - Size 必須為 2 次方: 為了充分利用二進制計算優勢
/// - Thread Safety: 只能用於兩個執行續兼溝通,多執行續會有競爭條件
///
template <typename T, size_t Capacity>
  requires std::is_move_constructible_v<T> && (Capacity > 0) &&
           ((Capacity & (Capacity - 1)) == 0)
class SPSCQueue {
 private:
  static constexpr size_t kCacheLineSize = 64;  ///<  Cache Line 大小

  static constexpr size_t kIndexMask =
      Capacity - 1;  ///< 因爲 Capacity 爲 2^n 表示二進制只有一個位置爲 1
                     ///< , 故 Capacity - 1 故得到剩下全是 1, 此時進行 &
                     ///< 運算會把原本數值 Capacity 以上的去掉只留下 Capacity
                     ///< 以下的數值, 這就是快速取餘

  alignas(kCacheLineSize) std::atomic<size_t> head_{0};
  alignas(kCacheLineSize) std::atomic<size_t> tail_{0};
  alignas(kCacheLineSize) std::array<T, Capacity> buffer_;

 public:
  // ----------------------------------------------------------------------------
  // MARK: 建構函數
  // ----------------------------------------------------------------------------

  SPSCQueue() = default;

  // ----------------------------------------------------------------------------
  // MARK: RAII
  // ----------------------------------------------------------------------------

  SPSCQueue(const SPSCQueue&) = delete;
  SPSCQueue& operator=(const SPSCQueue&) = delete;
  SPSCQueue(SPSCQueue&&) = delete;
  SPSCQueue& operator=(SPSCQueue&&) = delete;

  // ----------------------------------------------------------------------------
  // MARK: 狀態查詢
  // ----------------------------------------------------------------------------

  [[nodiscard]] size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] bool empty() const noexcept {
    size_t h = head_.load(std::memory_order_relaxed);
    size_t t = tail_.load(std::memory_order_relaxed);
    return h == t;
  }

  [[nodiscard]] size_t size() const noexcept {
    size_t h = head_.load(std::memory_order_relaxed);
    size_t t = tail_.load(std::memory_order_relaxed);
    // 因爲 t - h 是無符號整數運算, 故 t - h < 0 (概念上)
    // 會發生溢出, 變成 2^64 - (h - t), 對其進行 & 會得到
    // (h - t) % Capacity 的效果
    return (t - h) & kIndexMask;
  }

  // ----------------------------------------------------------------------------
  // MARK: 操作
  // ----------------------------------------------------------------------------

  /// @brief 嘗試推入元素（Move 語義）
  ///
  /// @return 成功或失敗
  [[nodiscard]] bool try_push(T&& value) noexcept {
    size_t cur_tail = tail_.load(std::memory_order_relaxed);
    size_t nxt_tail = (cur_tail + 1) & kIndexMask;

    if (nxt_tail == head_.load(std::memory_order_relaxed)) [[unlikely]] {
      return false;
    }

    buffer_[cur_tail] = std::move(value);
    tail_.store(nxt_tail, std::memory_order_release);
    return true;
  }

  /// @brief 嘗試推入元素
  /// @brief 嘗試推入元素（Copy 語義）
  ///
  /// @return 成功或失敗
  [[nodiscard]] bool try_push(const T& value) noexcept {
    size_t cur_tail = tail_.load(std::memory_order_relaxed);
    size_t nxt_tail = (cur_tail + 1) & kIndexMask;

    if (nxt_tail == head_.load(std::memory_order_relaxed)) [[unlikely]] {
      return false;
    }

    buffer_[cur_tail] = value;
    tail_.store(nxt_tail, std::memory_order_release);
    return true;
  }

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
    size_t cur_tail = tail_.load(std::memory_order_relaxed);
    size_t nxt_tail = (cur_tail + 1) & kIndexMask;

    if (nxt_tail == head_.load(std::memory_order_acquire)) [[unlikely]] {
      return false;
    }

    buffer_[cur_tail] = T(std::forward<Args>(args)...);
    tail_.store(nxt_tail, std::memory_order_release);
    return true;
  }

  /// @brief 嘗試取出
  ///
  /// @return 成功時包含元素，失敗時為 std::nullopt
  [[nodiscard]] std::optional<T> try_pop() noexcept {
    size_t cur_head = head_.load(std::memory_order_relaxed);
    if (cur_head == tail_.load(std::memory_order_acquire)) [[unlikely]] {
      return std::nullopt;
    }

    size_t nxt_head = (cur_head + 1) & kIndexMask;
    T value = std::move(buffer_[cur_head]);
    head_.store(nxt_head, std::memory_order_release);
    return std::move(value);
  }

  /// @brief 嘗試取出 (避免 optional 開銷)
  ///
  /// @param out 輸出參數 (會被 Move-assign)
  /// @return true = 成功, false = 佇列爲空
  [[nodiscard]] bool try_pop(T& out) noexcept {
    size_t cur_head = head_.load(std::memory_order_relaxed);
    if (cur_head == tail_.load(std::memory_order_acquire)) [[unlikely]] {
      return false;
    }

    out = std::move(buffer_[cur_head]);
    size_t nxt_head = (cur_head + 1) & kIndexMask;

    head_.store(nxt_head, std::memory_order_release);
    return true;
  }
};

}  // namespace tx::sync

#endif
