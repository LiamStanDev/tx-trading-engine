#include <benchmark/benchmark.h>

#include <thread>

#include "../util.hpp"
#include "onyx/bench/latency_recorder.hpp"
#include "onyx/sync/spsc_queue.hpp"
#include "onyx/sys/tsc_timer.hpp"

namespace {

using namespace onyx::sync;
using namespace onyx::bench;
using namespace onyx::sys;

}  // namespace

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
    ->Name("BM_SPSC/Latency/8B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Latency_PayloadSize, Payload64)
    ->Name("BM_SPSC/Latency/64B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Latency_PayloadSize, Payload256)
    ->Name("BM_SPSC/Latency/256B")
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
    ->Name("BM_SPSC/Sustained/8B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Sustained, Payload64)
    ->Name("BM_SPSC/Sustained/64B")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Sustained, Payload256)
    ->Name("BM_SPSC/Sustained/256B")
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
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Burst, Payload8)->Name("BM_SPSC/Burst/8B")->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Burst, Payload64)->Name("BM_SPSC/Burst/64B")->UseRealTime();
BENCHMARK_TEMPLATE(BM_SPSC_Throughput_Burst, Payload256)->Name("BM_SPSC/Burst/256B")->UseRealTime();
