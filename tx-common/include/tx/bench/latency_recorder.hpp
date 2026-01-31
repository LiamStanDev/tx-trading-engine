#ifndef TX_TRADING_ENGINE_BENCH_LATENCY_RECORDER_HPP
#define TX_TRADING_ENGINE_BENCH_LATENCY_RECORDER_HPP

#include <cstdint>
#include <vector>

namespace tx::bench {

/// @brief Latency 統計結果
///
struct LatencyStats {
  // ----------------------------------------------------------------------------
  // Percentiles
  // ----------------------------------------------------------------------------

  double p50_ns;   ///< 中位數
  double p90_ns;   ///< 90% 延遲上界
  double p99_ns;   ///< 99% 延遲上界
  double p999_ns;  ///< 99.9% 延遲上界

  // ----------------------------------------------------------------------------
  // Meta
  // ----------------------------------------------------------------------------
  size_t total_samples;  ///< 總樣本數
};

/// @brief Latency 測量數據收集器
///
/// ### Usage
/// ```cpp
/// LatencyRecorder recorder(1'000'000);  // 預分配 1M 樣本空間
///
/// for (size_t i = 0; i < iterations; ++i) {
///   uint64_t t0 = TSCTimer::now();
///   // ... 被測代碼 ...
///   uint64_t t1 = TSCTimer::now();
///   recorder.record(t1 - t0);  // 記錄 cycles
/// }
///
/// auto stats = recorder.report();
/// // stats.p99_ns, stats.histogram, ...
/// ```
///
class LatencyRecorder {
 private:
  std::vector<uint64_t> samples_;  ///< 原始測量數據 (TSC cycles)

 public:
  /// @brief 構造函數
  ///
  /// @param reserve_size 預分配的樣本容量
  ///
  explicit LatencyRecorder(size_t reserve_size = 1'000'000) {
    samples_.reserve(reserve_size);
  }

  /// @brief 記錄一次測量
  ///
  /// @param cycles TSC cycles (from TSCTimer::now())
  ///
  inline void record(uint64_t cycles) { samples_.push_back(cycles); }

  /// @brief 取得當前樣本數
  ///
  size_t sample_count() const { return samples_.size(); }

  /// @brief 取得樣本
  const std::vector<uint64_t>& samples() const { return samples_; }

  /// @brief 清空所有樣本（重用 recorder）
  ///
  void reset() { samples_.clear(); }

  /// @brief 打印報告
  ///
  void report() {
    LatencyStats stats = compute_stats();
    print_latency_stats(stats);
  }

  /// @brief 計算統計指標
  ///
  LatencyStats compute_stats();

 private:
  void print_latency_stats(const LatencyStats& stats);
};

}  // namespace tx::bench

#endif  // TX_TRADING_ENGINE_BENCH_LATENCY_RECORDER_HPP
