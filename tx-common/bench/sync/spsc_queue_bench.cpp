#include <benchmark/benchmark.h>
#include <x86emu.h>
#include <x86intrin.h>

#include <atomic>
#include <thread>

#include "../util.hpp"
#include "tx/sync/spsc_queue.hpp"
#include "tx/sys/tsc_timer.hpp"

namespace tx::sync::bench {

// ----------------------------------------------------------------------------
// MARK: Single-Thread Latency
// ----------------------------------------------------------------------------
static void BM_SPSCQueue_Latency_SingleThread(benchmark::State& state) {
  LatencyRecorder recorder;

  SPSCQueue<int, 1024> queue;

  for (auto _ : state) {
    state.PauseTiming();
    uint64_t t0 = sys::TSCTimer::now();

    bool push_ok = queue.try_push(42);
    auto val = queue.try_pop();
    uint64_t t1 = sys::TSCTimer::now();

    benchmark::DoNotOptimize(push_ok);
    benchmark::DoNotOptimize(val);

    recorder.record(t1 - t0);
  }
  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);
  state.ResumeTiming();
}

BENCHMARK(BM_SPSCQueue_Latency_SingleThread)
    ->Iterations(kBenchmarkIterationSize);

// ----------------------------------------------------------------------------
// MARK: Muti-Thread Latency
// ----------------------------------------------------------------------------

static void BM_SPSCQueue_Latency_Multithread(benchmark::State& state) {
  LatencyRecorder recorder;
  SPSCQueue<uint64_t, 1024> queue;

  std::atomic<bool> start{false};          // 消費者啟動標誌
  std::atomic<bool> producer_done{false};  // 生產者完成

  std::thread producer([&] {
    // 等待 consumer
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    for (auto _ : state) {
      uint64_t send_time = sys::TSCTimer::now();
      while (!queue.try_push(send_time)) {
        _mm_pause();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  // Consumer main thread
  start.store(true, std::memory_order_release);

  while (true) {
    if (auto msg = queue.try_pop()) {
      uint64_t recv_time = sys::TSCTimer::now();
      uint64_t latency_cycles = recv_time - *msg;

      recorder.record(latency_cycles);
    } else {  // Queue 無資料
      // 且生產者完成
      if (producer_done.load(std::memory_order_acquire)) {
        break;
      }

      _mm_pause();
    }
  }

  producer.join();

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);
}

BENCHMARK(BM_SPSCQueue_Latency_Multithread)
    ->Iterations(kBenchmarkIterationSize);

}  // namespace tx::sync::bench
