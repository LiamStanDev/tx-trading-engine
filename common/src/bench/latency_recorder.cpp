#include "onyx/bench/latency_recorder.hpp"

#include <fmt/format.h>

#include <algorithm>

#include "onyx/sys/tsc_timer.hpp"

namespace onyx::bench {

LatencyStats LatencyRecorder::compute_stats() {
  if (samples_.empty()) {
    return {};  // 返回空統計
  }

  std::sort(samples_.begin(), samples_.end());

  const size_t n = samples_.size();

  auto percentile = [&](double p) -> double {
    size_t idx = static_cast<size_t>(std::ceil(static_cast<double>(n) * p)) - 1;
    idx = std::min(idx, n - 1);  // 防止越界
    return sys::TSCTimer::cycles_to_ns(samples_[idx]);
  };

  return LatencyStats{
      .p50_ns = percentile(0.50),
      .p90_ns = percentile(0.90),
      .p99_ns = percentile(0.99),
      .p999_ns = percentile(0.999),
      .total_samples = n,
  };
}

void LatencyRecorder::print_latency_stats(const LatencyStats& stats) {
  fmt::println("{}", std::string(60, '-'));

  auto print_metric = [](const char* name, double value, const char* desc) {
    fmt::println("{:<10} {:>12.2f}ns    {}", name, value, desc);
  };

  print_metric("p50", stats.p50_ns, "50% of ops faster than this");
  print_metric("p90", stats.p90_ns, "90% of ops faster than this");
  print_metric("p99", stats.p99_ns, "99% of ops faster than this");
  print_metric("p999", stats.p999_ns, "99.9% of ops faster than this");
  fmt::println("{}", std::string(60, '-'));
  fmt::println("Total samples: {:L}", stats.total_samples);
}

}  // namespace onyx::bench
