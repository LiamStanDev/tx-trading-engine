#ifndef TX_TRADING_ENGINE_UTIL_HPP
#define TX_TRADING_ENGINE_UTIL_HPP

#include <benchmark/benchmark.h>
#include <fmt/format.h>

#include "tx/bench/latency_recorder.hpp"

void register_latency_stats(benchmark::State& state,
                            tx::bench::LatencyRecorder& recorder) {
  auto stats = recorder.compute_stats();
  // state.counters["p50"] = stats.p50_ns;
  // state.counters["p90"] = stats.p90_ns;
  // state.counters["p99"] = stats.p99_ns;
  // state.counters["p999"] = stats.p999_ns;

  std::string label =
      fmt::format("P50: {:>8.2f}ns | P99: {:>8.2f}ns | P999: {:>8.2f}ns",
                  stats.p50_ns, stats.p90_ns, stats.p99_ns, stats.p999_ns);

  state.SetLabel(label);
}

#endif
