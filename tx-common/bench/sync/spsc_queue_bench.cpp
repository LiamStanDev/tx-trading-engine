#include <benchmark/benchmark.h>

#include <thread>

#include "../util.hpp"
#include "tx/bench/latency_recorder.hpp"
#include "tx/sync/spsc_queue.hpp"
#include "tx/sys/tsc_timer.hpp"

using namespace tx::sync;
using namespace tx::bench;
using namespace tx::sys;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

// 8 bytes: 純粹的 timestamp
struct Payload8 {
  uint64_t timestamp;
};

// 64 bytes: 模擬一個簡單的 Order
struct alignas(64) Payload64 {
  uint64_t timestamp;
  uint64_t order_id;
  uint32_t price;
  uint32_t quantity;
  char symbol[8];
  uint8_t side;         // 'B' or 'S'
  uint8_t padding[31];  // 填充到 64 bytes
};

// 256 bytes: 模擬 Market Data (Level 2 Depth)
struct alignas(64) Payload256 {
  uint64_t timestamp;
  char symbol[16];
  struct Level {
    uint32_t price;
    uint32_t quantity;
  } bids[10], asks[10];  // 10 levels each
  uint8_t padding[72];   // 填充到 256 bytes
};

// Helper: 驗證大小
static_assert(sizeof(Payload8) == 8);
static_assert(sizeof(Payload64) == 64);
static_assert(sizeof(Payload256) == 256);

// ----------------------------------------------------------------------------
// Latency
// ----------------------------------------------------------------------------

template <typename T>
static void BM_SPSC_Latency_PayloadSize(benchmark::State& state) {
  constexpr size_t kQueueSize = 1024;

  SPSCQueue<T, kQueueSize> queue_fwd;  // A → B
  SPSCQueue<T, kQueueSize> queue_bwd;  // B → A

  LatencyRecorder recorder(state.max_iterations);
  std::atomic<bool> stop{false};

  // Echo thread: 收到後立刻彈回
  std::thread echo_thread([&]() {
    T value;
    while (!stop.load(std::memory_order_relaxed)) {
      if (queue_fwd.try_pop(value)) {
        while (!queue_bwd.try_push(std::move(value)));
      }
    }
  });

  // Main thread: 測量 Round-Trip
  for (auto _ : state) {
    T payload;
    if constexpr (requires { payload.timestamp; }) {
      payload.timestamp = TSCTimer::now();  // 記錄發送時間
    }

    uint64_t t0 = TSCTimer::now();

    // 送出
    while (!queue_fwd.try_push(std::move(payload)));

    // 等待回應
    T reply;
    while (!queue_bwd.try_pop(reply));

    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
  }

  stop.store(true);
  echo_thread.join();

  register_latency_stats(state, recorder);
}

BENCHMARK_TEMPLATE(BM_SPSC_Latency_PayloadSize, Payload8)
    ->Name("BM_SPSC_Latency/8B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Latency_PayloadSize, Payload64)
    ->Name("BM_SPSC_Latency/64B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Latency_PayloadSize, Payload256)
    ->Name("BM_SPSC_Latency/256B")
    ->UseRealTime();

// ----------------------------------------------------------------------------
// Throughput
// ----------------------------------------------------------------------------

template <typename T>
static void BM_SPSC_Throughput_Sustained(benchmark::State& state) {
  constexpr size_t kQueueSize = 4096;
  SPSCQueue<T, kQueueSize> queue;

  std::atomic<bool> stop{false};
  std::atomic<size_t> consumed{0};

  // Consumer 執行緒
  std::thread consumer([&]() {
    T value;
    while (!stop.load(std::memory_order_relaxed)) {
      if (queue.try_pop(value)) {
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    }
    // 清空剩餘
    while (queue.try_pop(value)) {
      consumed.fetch_add(1, std::memory_order_relaxed);
    }
  });

  // Producer (主執行緒)
  size_t produced = 0;
  for (auto _ : state) {
    T payload;
    // 如果有 timestamp,記錄時間
    if constexpr (requires { payload.timestamp; }) {
      payload.timestamp = TSCTimer::now();
    }

    while (!queue.try_push(std::move(payload))) {
      // Spin if queue is full
    }
    ++produced;
  }

  stop.store(true);
  consumer.join();

  // 報告吞吐量
  state.SetItemsProcessed(consumed.load());
  state.counters["produced"] = produced;
  state.counters["consumed"] = consumed.load();
}

// 註冊不同 Payload Size 的測試
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Sustained, Payload8)
    ->Name("BM_SPSC_Throughput_Sustained/8B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Sustained, Payload64)
    ->Name("BM_SPSC_Throughput_Sustained/64B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Sustained, Payload256)
    ->Name("BM_SPSC_Throughput_Sustained/256B")
    ->UseRealTime();

template <typename T>
static void BM_SPSC_Throughput_Burst(benchmark::State& state) {
  constexpr size_t kQueueSize = 4096;
  constexpr size_t kBurstSize = 1000;  // 每次突發 1000 個訊息

  SPSCQueue<T, kQueueSize> queue;

  std::atomic<bool> stop{false};
  std::atomic<size_t> consumed{0};

  // Consumer 全力消費
  std::thread consumer([&]() {
    T value;
    while (!stop.load(std::memory_order_relaxed)) {
      if (queue.try_pop(value)) {
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    }
    while (queue.try_pop(value)) {
      consumed.fetch_add(1, std::memory_order_relaxed);
    }
  });

  for (auto _ : state) {
    // 快速塞入 kBurstSize 個訊息
    for (size_t i = 0; i < kBurstSize; ++i) {
      T payload;
      while (!queue.try_push(std::move(payload)));
    }
  }

  stop.store(true);
  consumer.join();

  state.SetItemsProcessed(consumed.load());
  state.counters["burst_size"] = kBurstSize;
}
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Burst, Payload8)
    ->Name("BM_SPSC_Throughput_Burst/8B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Burst, Payload64)
    ->Name("BM_SPSC_Throughput_Burst/64B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Burst, Payload256)
    ->Name("BM_SPSC_Throughput_Burst/256B")
    ->UseRealTime();
