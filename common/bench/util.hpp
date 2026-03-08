#ifndef TX_TRADING_ENGINE_UTIL_HPP
#define TX_TRADING_ENGINE_UTIL_HPP

#include <benchmark/benchmark.h>
#include <fmt/format.h>

#include "onyx/bench/latency_recorder.hpp"

namespace {
inline std::string format_duration(double ns) {
  if (ns < 1000.0) {
    return fmt::format("{:>8.2f}ns", ns);
  } else if (ns < 1'000'000.0) {
    return fmt::format("{:>8.2f}us", ns / 1000.0);
  } else if (ns < 1'000'000'000.0) {
    return fmt::format("{:>8.2f}ms", ns / 1'000'000.0);
  } else {
    return fmt::format("{:>8.2f}s ", ns / 1'000'000'000.0);
  }
}
}  // namespace

inline void register_latency_stats(benchmark::State& state,
                                   onyx::bench::LatencyRecorder& recorder) {
  auto stats = recorder.compute_stats();
  // state.counters["p50"] = stats.p50_ns;
  // state.counters["p90"] = stats.p90_ns;
  // state.counters["p99"] = stats.p99_ns;
  // state.counters["p999"] = stats.p999_ns;

  std::string label = fmt::format("P50: {} | P99: {} | P99.9: {}", format_duration(stats.p50_ns),
                                  format_duration(stats.p99_ns), format_duration(stats.p999_ns));
  state.SetLabel(label);
}

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

#endif
