#ifndef TX_TRADING_ENGINE_SYS_TSCTIMER_HPP
#define TX_TRADING_ENGINE_SYS_TSCTIMER_HPP

#include <x86intrin.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>

#include "tx/sys/cpu_affinity.hpp"

namespace tx::sys {

/// @brief CPU Cycle 計時器
///
/// 可以提供超越 `std::chrono` 的精度, 利用 CPU 的計時器來達到
/// 更高精度的準確度
///
class TSCTimer {
 private:
  inline static double ns_per_cycle_{0};
  inline static bool calibrated_{false};

 public:
  static void calibrate(std::chrono::milliseconds duration =
                            std::chrono::milliseconds(100)) noexcept {
    // 綁定到指定 CPU
    auto _ = CPUAffinity::pin_to_cpu(0);

    // 多次測量
    constexpr int iterations = 5;
    double sum_ns_per_cycle = 0;

    for (int i = 0; i < iterations; ++i) {
      auto t1_chrono = std::chrono::high_resolution_clock::now();
      uint64_t t1_tsc = now();

      // Busy-wait 而非 sleep
      auto target = t1_chrono + duration;
      while (std::chrono::high_resolution_clock::now() < target) {
        _mm_pause();  // Hint to CPU: we're spinning
      }

      uint64_t t2_tsc = now();
      auto t2_chrono = std::chrono::high_resolution_clock::now();

      double ns_elapsed =
          std::chrono::duration<double, std::nano>(t2_chrono - t1_chrono)
              .count();
      sum_ns_per_cycle += ns_elapsed / static_cast<double>(t2_tsc - t1_tsc);
    }

    ns_per_cycle_ = sum_ns_per_cycle / iterations;
    calibrated_ = true;
  }

  static inline uint64_t now() noexcept {
    _mm_lfence();              // 確保讀取之前所有指令都已執行完成完成，防止 CPU
                               // 預先執行以下代碼
    uint64_t tsc = __rdtsc();  // read time stamp counter
    _mm_lfence();              // 確保 rdtsc 已經執行完成
    return tsc;
  }

  static inline double cycles_to_ns(uint64_t cycles) noexcept {
    if (!calibrated_) [[unlikely]] {
      std::cerr << "[TSCTimer] won't call calibrate() first\n";
      std::terminate();
    }
    return static_cast<double>(cycles) * ns_per_cycle_;
  }
};

}  // namespace tx::sys

#endif
