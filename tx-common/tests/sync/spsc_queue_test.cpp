#include "tx/sync/spsc_queue.hpp"

#include <gtest/gtest.h>

namespace tx::sync::test {

using Queue = SPSCQueue<int, 8>;

// =============================
// 基本功能測試
// =============================

TEST(SPSCQueueTest, InitialState) {
  Queue queue;
  EXPECT_EQ(queue.capacity(), 8);
  EXPECT_EQ(queue.size(), 0);
  EXPECT_TRUE(queue.empty());
}

TEST(SPSCQueueTest, PushAndPop) {
  Queue queue;
  EXPECT_TRUE(queue.try_push(42));
  EXPECT_EQ(queue.size(), 1);
  EXPECT_FALSE(queue.empty());
  auto value = queue.try_pop();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 42);
  EXPECT_TRUE(queue.empty());
}

TEST(SPSCQueueTest, PushCopy) {
  Queue queue;
  int x = 99;
  EXPECT_TRUE(queue.try_push(x));  // Copy
  auto value = queue.try_pop();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 99);
  EXPECT_EQ(x, 99);  // 原值不變
}

TEST(SPSCQueueTest, Emplace) {
  SPSCQueue<std::string, 8> queue;
  EXPECT_TRUE(
      queue.try_emplace(static_cast<size_t>(5), '!'));  // std::string(5, '!')
  auto value = queue.try_pop();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "!!!!!");
}
// =============================
// 邊界條件測試
// =============================
TEST(SPSCQueueTest, FullQueue) {
  Queue queue;
  // 填滿佇列（注意：保留一個空位，所以只能插入 Capacity - 1 個）
  for (int i = 0; i < 7; ++i) {
    EXPECT_TRUE(queue.try_push(i)) << "Failed at i=" << i;
  }
  EXPECT_EQ(queue.size(), 7);
  // 第 8 個應該失敗（佇列已滿）
  EXPECT_FALSE(queue.try_push(999));
}
TEST(SPSCQueueTest, EmptyQueue) {
  Queue queue;
  auto value = queue.try_pop();
  EXPECT_FALSE(value.has_value());
  int out;
  EXPECT_FALSE(queue.try_pop(out));
}
TEST(SPSCQueueTest, WrapAround) {
  Queue queue;
  // 測試 Ring Buffer 循環
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 7; ++i) {
      EXPECT_TRUE(queue.try_push(i + round * 10));
    }
    for (int i = 0; i < 7; ++i) {
      auto value = queue.try_pop();
      ASSERT_TRUE(value.has_value());
      EXPECT_EQ(*value, i + round * 10);
    }
    EXPECT_TRUE(queue.empty());
  }
}

// =============================
// Move Semantics 測試
// =============================
struct MoveOnlyType {
  int value;
  explicit MoveOnlyType(int v = 0) : value(v) {}
  // Move-only
  MoveOnlyType(const MoveOnlyType&) = delete;
  MoveOnlyType& operator=(const MoveOnlyType&) = delete;
  MoveOnlyType(MoveOnlyType&& other) noexcept : value(other.value) {
    other.value = -1;
  }
  MoveOnlyType& operator=(MoveOnlyType&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
};

TEST(SPSCQueueTest, MoveOnly) {
  SPSCQueue<MoveOnlyType, 8> queue;
  EXPECT_TRUE(queue.try_push(MoveOnlyType(42)));
  auto value = queue.try_pop();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->value, 42);
}

// =============================
// 性能特徵測試
// =============================
TEST(SPSCQueueTest, CapacityIsPowerOfTwo) {
  // 編譯期檢查：Capacity 必須是 2 的冪次
  SPSCQueue<int, 1> q1;
  SPSCQueue<int, 2> q2;
  SPSCQueue<int, 4> q4;
  SPSCQueue<int, 1024> q1024;
  // 以下應該編譯失敗（取消註解測試）:
  // SPSCQueue<int, 3> q3;      // ❌ 不是 2 的冪次
  // SPSCQueue<int, 100> q100;  // ❌ 不是 2 的冪次
}

TEST(SPSCQueueTest, LargeCapacity) {
  SPSCQueue<int, 65536> queue;  // 64K 元素
  // Push 10000 個
  for (int i = 0; i < 10000; ++i) {
    EXPECT_TRUE(queue.try_push(i));
  }
  // Pop 10000 個
  for (int i = 0; i < 10000; ++i) {
    auto value = queue.try_pop();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, i);
  }
}

// =============================
// 多執行緒測試
// =============================

TEST(SPSCQueueMTTest, ProducerConsumer) {
  constexpr size_t kNumMessages = 1'000'000;
  SPSCQueue<int, 1024> queue;
  std::atomic<bool> done{false};
  std::atomic<size_t> consumed{0};

  // Consumer thread
  std::thread consumer([&] {
    while (!done.load(std::memory_order_acquire) || !queue.empty()) {
      if (auto value = queue.try_pop()) {
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  // Producer thread
  std::thread producer([&] {
    for (size_t i = 0; i < kNumMessages; ++i) {
      while (!queue.try_push(static_cast<int>(i))) {
        // Spin
      }
    }
    done.store(true, std::memory_order_release);
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(consumed.load(), kNumMessages);
}

TEST(SPSCQueueMTTest, Ordering) {
  constexpr size_t kNumMessages = 100'000;
  SPSCQueue<int, 1024> queue;
  std::vector<int> consumed;
  consumed.reserve(kNumMessages);
  std::atomic<bool> done{false};

  std::thread consumer([&] {
    while (!done.load(std::memory_order_acquire) || !queue.empty()) {
      if (auto value = queue.try_pop()) {
        consumed.push_back(*value);
      }
    }
  });

  std::thread producer([&] {
    for (size_t i = 0; i < kNumMessages; ++i) {
      while (!queue.try_push(static_cast<int>(i))) {
      }
    }
    done.store(true, std::memory_order_release);
  });

  producer.join();
  consumer.join();

  // 驗證順序（FIFO）
  ASSERT_EQ(consumed.size(), kNumMessages);
  for (size_t i = 0; i < kNumMessages; ++i) {
    EXPECT_EQ(consumed[i], static_cast<int>(i)) << "Mismatch at index " << i;
  }
}
}  // namespace tx::sync::test
