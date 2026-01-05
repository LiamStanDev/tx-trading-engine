#ifndef TX_TRADING_ENGINE_SYNC_SPSC_QUEUE_HPP
#define TX_TRADING_ENGINE_SYNC_SPSC_QUEUE_HPP

#include <array>
#include <atomic>
#include <concepts>
#include <optional>

namespace tx::sync {

template <typename T, size_t Capacity>
  requires std::is_move_constructible_v<T> && (Capacity > 0) &&
           ((Capacity & (Capacity - 1)) == 0)
class SPSCQueue {
 private:
  /// @brief Cache Line 大小
  static constexpr size_t kCacheLineSize = 64;
  /// @brief 作爲快速取餘運算使用
  /// @details
  /// 因爲 Capacity 爲 2^n 表示二進制只有一個位置爲 1
  /// , 故 Capacity - 1 故得到剩下全是 1, 此時進行 &
  /// 運算會把原本數值 Capacity 以上的去掉只留下 Capacity
  /// 以下的數值, 這就是快速取餘
  static constexpr size_t kIndexMask = Capacity - 1;

  alignas(kCacheLineSize) std::atomic<size_t> head_{0};
  char pad1_[kCacheLineSize - sizeof(std::atomic<size_t>)];
  alignas(kCacheLineSize) std::atomic<size_t> tail_{0};
  char pad2_[kCacheLineSize - sizeof(std::atomic<size_t>)];
  alignas(kCacheLineSize) std::array<T, Capacity> buffer_;

 public:
  /// ==========================
  /// 建構函數
  /// ==========================
  SPSCQueue() = default;

  /// ==========================
  /// RAII
  /// ==========================
  SPSCQueue(const SPSCQueue&) = delete;
  SPSCQueue& operator=(const SPSCQueue&) = delete;
  SPSCQueue(SPSCQueue&&) = delete;
  SPSCQueue& operator=(SPSCQueue&&) = delete;

  /// ==========================
  /// 狀態查詢
  /// ==========================

  [[nodiscard]] size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] bool empty() const noexcept {
    size_t h = head_.load(std::memory_order_acquire);
    size_t t = tail_.load(std::memory_order_acquire);
    return h == t;
  }

  [[nodiscard]] size_t size() const noexcept {
    size_t h = head_.load(std::memory_order_acquire);
    size_t t = tail_.load(std::memory_order_acquire);
    // 因爲 t - h 是無符號整數運算, 故 t - h < 0 (概念上)
    // 會發生溢出, 變成 2^64 - (h - t), 對其進行 & 會得到
    // (h - t) % Capacity 的效果
    return (t - h) & kIndexMask;
  }

  /// ==========================
  /// 操作
  /// ==========================

  /// @brief 嘗試推入元素（Move 語義）
  [[nodiscard]] bool try_push(T&& value) noexcept {
    size_t cur_tail = tail_.load(std::memory_order_relaxed);
    size_t nxt_tail = (cur_tail + 1) & kIndexMask;

    if (nxt_tail == head_.load(std::memory_order_acquire)) [[unlikely]] {
      return false;
    }

    buffer_[cur_tail] = std::move(value);
    tail_.store(nxt_tail, std::memory_order_release);
    return true;
  }

  /// @brief 嘗試推入元素（Copy 語義）
  [[nodiscard]] bool try_push(const T& value) noexcept {
    size_t cur_tail = tail_.load(std::memory_order_relaxed);
    size_t nxt_tail = (cur_tail + 1) & kIndexMask;

    if (nxt_tail == head_.load(std::memory_order_acquire)) [[unlikely]] {
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
