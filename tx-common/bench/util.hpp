#ifndef TX_TRADING_ENGINE_BENCH_UTIL_HPP
#define TX_TRADING_ENGINE_BENCH_UTIL_HPP

#include <benchmark/benchmark.h>
#include <fmt/format.h>

#include <cstdint>
#include <vector>

#include "tx/sys/tsc_timer.hpp"

class LatencyBenchmarkReporter : public benchmark::ConsoleReporter {
 public:
  bool ReportContext(const Context& context) override {
    bool result = ConsoleReporter::ReportContext(context);

    // 加入自訂的 header
    fmt::println("{}", std::string(60, '='));
    fmt::println("Latency Benchmark Results");
    fmt::println("{}", std::string(60, '='));

    return result;
  }

  void ReportRuns(const std::vector<Run>& reports) override {
    for (const auto& run : reports) {
      if (run.skipped) continue;

      // 格式化輸出
      fmt::println("{}", run.benchmark_name());
      fmt::println("{}", std::string(60, '-'));

      auto print_metric = [&](const char* name, const char* desc) {
        auto it = run.counters.find(name);
        if (it != run.counters.end()) {
          // 1. {:<10}  : 左對齊名稱，佔 10 格
          // 2. {:>10.2f}: 修正了原本錯誤的 {.2f}，加上冒號並設為右對齊，佔 10
          // 格
          fmt::println("{:<10} {:>10.2f}ns    {}", name,
                       static_cast<double>(it->second), desc);
        }
      };

      print_metric("mean", "Average latency");
      print_metric("p50", "50% of ops faster than this");
      print_metric("p90", "90% of ops faster than this");
      print_metric("p99", "99% of ops faster than this");
      print_metric("p999", "99.9% of ops faster than this");
      print_metric("max", "Worst-case spike");

      // 底部處理
      fmt::println("{:-^60}", "");

      // 公式: (1 sec / latency_ns) * 10^9 / 10^6 = 1000 / latency_ns
      double m_ops = 1000.0 / run.GetAdjustedRealTime();
      fmt::println("Throughput: {:.2f} M ops/s", m_ops);
    }
  }
};

inline constexpr size_t kBenchmarkIterationSize = 10'000'000;

class LatencyRecorder {
 private:
  std::vector<uint64_t> samples_;
  size_t cur_idx_{0};

 public:
  explicit LatencyRecorder(size_t reserve_size = kBenchmarkIterationSize) {
    samples_.resize(reserve_size);
  }

  void record(uint64_t cycles) { samples_[cur_idx_++] = cycles; }

  struct Stats {
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double p999_ns;
    double max_ns;
    double mean_ns;
  };

  Stats compute_stats() {
    if (samples_.empty()) {
      return {0, 0, 0, 0, 0, 0};
    }

    // 排序以計算分位數
    std::sort(samples_.begin(), samples_.end());

    auto percentile = [this](double p) -> double {
      size_t idx = static_cast<size_t>(samples_.size() * p);
      idx = std::min(idx, samples_.size() - 1);
      return tx::sys::TSCTimer::cycles_to_ns(samples_[idx]);
    };

    double sum = 0;
    for (uint64_t c : samples_) {
      sum += tx::sys::TSCTimer::cycles_to_ns(c);
    }

    return Stats{.p50_ns = percentile(0.50),
                 .p90_ns = percentile(0.90),
                 .p99_ns = percentile(0.99),
                 .p999_ns = percentile(0.999),
                 .max_ns = tx::sys::TSCTimer::cycles_to_ns(samples_.back()),
                 .mean_ns = sum / samples_.size()};
  }

  size_t sample_count() const { return samples_.size(); }
};

inline void report_latency_stats(benchmark::State& state,
                                 const LatencyRecorder::Stats& stats) {
  state.counters["p50"] = stats.p50_ns;
  state.counters["p90"] = stats.p90_ns;
  state.counters["p99"] = stats.p99_ns;
  state.counters["p999"] = stats.p999_ns;
  state.counters["max"] = stats.max_ns;
  state.counters["mean"] = stats.mean_ns;
}

#endif
