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

 public:
  static inline uint64_t now() noexcept {
    _mm_lfence();              // 確保讀取之前所有指令都已執行完成完成，防止 CPU
                               // 預先執行以下代碼
    uint64_t tsc = __rdtsc();  // read time stamp counter
    _mm_lfence();              // 確保 rdtsc 已經執行完成
    return tsc;
  }

  static inline double cycles_to_ns(uint64_t cycles) noexcept {
    return static_cast<double>(cycles) * ns_per_cycle_;
  }

 private:
  // 透過靜態變數，在初始化自定進行校準
  inline static int _auto_calibrate = []() {
    // 綁定到指定 CPU
    (void)CPUAffinity::pin_to_cpu(0);

    // 多次測量
    auto t1 = std::chrono::high_resolution_clock::now();
    uint64_t start_tsc = __rdtsc();

    // 忙等待 100ms
    auto target = t1 + std::chrono::milliseconds(100);
    while (std::chrono::high_resolution_clock::now() < target) {
      _mm_pause();
    }

    uint64_t end_tsc = __rdtsc();
    auto t2 = std::chrono::high_resolution_clock::now();

    double ns_elapsed =
        std::chrono::duration<double, std::nano>(t2 - t1).count();

    ns_per_cycle_ = ns_elapsed / static_cast<double>(end_tsc - start_tsc);

    (void)CPUAffinity::clear_affinity();
    return 0;
  }();
};

}  // namespace tx::sys

#endif
