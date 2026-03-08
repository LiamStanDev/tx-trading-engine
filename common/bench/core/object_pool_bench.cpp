#include <benchmark/benchmark.h>

#include "../util.hpp"
#include "onyx/core/object_pool.hpp"
#include "onyx/sys/tsc_timer.hpp"

using namespace onyx::core;
using namespace onyx::bench;
using namespace onyx::sys;

static void BM_Pool_AllocDealloc(benchmark::State& state) {
  ObjectPool<Payload256, 1024> pool;
  LatencyRecorder recorder(state.max_iterations);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();
    Payload256* ptr = pool.allocate();
    benchmark::DoNotOptimize(ptr);
    pool.deallocate(ptr);
    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
  }

  register_latency_stats(state, recorder);
}

BENCHMARK(BM_Pool_AllocDealloc)->Name("ObjectPool/Cycle/Pool")->UseRealTime()->MinTime(0.3);

static void BM_New_AllocDealloc(benchmark::State& state) {
  LatencyRecorder recorder(state.max_iterations);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();
    Payload256* ptr = new Payload256;
    benchmark::DoNotOptimize(ptr);
    delete ptr;
    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
  }

  register_latency_stats(state, recorder);
}

BENCHMARK(BM_New_AllocDealloc)->Name("ObjectPool/Cycle/New")->UseRealTime()->MinTime(0.3);

static void BM_Pool_Brust(benchmark::State& state) {
  ObjectPool<Payload256, 1024> pool;
  LatencyRecorder recorder(state.max_iterations);

  std::vector<Payload256*> ptrs;
  ptrs.reserve(1024);
  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();
    for (size_t i = 0; i < 1024; ++i) {
      Payload256* ptr = pool.allocate();
      benchmark::DoNotOptimize(ptr);
      ptrs.push_back(ptr);
    }

    for (Payload256* ptr : ptrs) {
      pool.deallocate(ptr);
    }
    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
    ptrs.clear();
  }

  register_latency_stats(state, recorder);
}

BENCHMARK(BM_Pool_Brust)->Name("ObjectPool/Burst/Pool")->UseRealTime()->MinTime(0.3);

static void BM_New_Brust(benchmark::State& state) {
  LatencyRecorder recorder(state.max_iterations);

  std::vector<Payload256*> ptrs;
  ptrs.reserve(1024);
  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();
    for (size_t i = 0; i < 1024; ++i) {
      Payload256* ptr = new Payload256;
      benchmark::DoNotOptimize(ptr);
      ptrs.push_back(ptr);
    }
    for (Payload256* ptr : ptrs) {
      delete (ptr);
    }
    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
    ptrs.clear();
  }

  register_latency_stats(state, recorder);
}

BENCHMARK(BM_New_Brust)->Name("ObjectPool/Burst/New")->UseRealTime()->MinTime(0.3);
