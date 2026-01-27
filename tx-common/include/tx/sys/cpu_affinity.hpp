#ifndef TX_TRADING_ENGINE_SYS_CPU_AFFINITY_HPP
#define TX_TRADING_ENGINE_SYS_CPU_AFFINITY_HPP

#include <span>
#include <vector>

#include "tx/error.hpp"
namespace tx::sys {

/// @brief CPU Affinity 管理類別
///
/// 所有方法都是 static，不需要實例化
class CPUAffinity {
 public:
  CPUAffinity() = delete;  // static class

  // ----------------------------------------------------------------------------
  // Affinity Control
  // ----------------------------------------------------------------------------

  /// @brief 綁定當前 thread 到單一 CPU
  ///
  /// @param cpu_id CPU編號
  /// @return 成功或錯誤
  /// @note 此操作會清除原有的 affinity 設置
  ///
  [[nodiscard]] static Result<> pin_to_cpu(size_t cpu_id) noexcept;

  /// @brief 綁定當前 thread 到多個 CPU
  ///
  /// @param cpu_ids CPU編號列表
  /// @return 成功或錯誤
  /// @note 此操作會清除原有的 affinity 設置
  ///
  [[nodiscard]] static Result<> pin_to_cpus(
      std::span<const size_t> cpu_ids) noexcept;

  /// @brief 取得當前 thread 的 CPU affinity
  ///
  /// @return CPU ID 列表或者錯誤碼
  ///
  [[nodiscard]] static Result<std::vector<size_t>> get_affinity() noexcept;

  /// @brief 清除當前 thread 的 CPU affinity 設置，允許所有可用 CPU 上執行
  ///
  /// @return 成功或錯誤
  ///
  [[nodiscard]] static Result<> clear_affinity() noexcept;

  // ----------------------------------------------------------------------------
  // 系統資訊
  // ----------------------------------------------------------------------------

  /// @brief 取得當前 CPU 個數
  /// @return CPU 個數
  ///
  /// @note
  /// 這包含 online 和 offline 的 CPU，實際可用的 CPU 請用
  /// `get_available_cpus()`
  ///
  static size_t get_cpu_count() noexcept;

  /// @brief 取得系統可用的 CPU 列表
  /// @return CPU ID 列表
  ///
  /// @note
  /// - 某些 CPU 可能被 isolcpus 或 cpuset 隔離，不在此列表中
  /// - 若無法讀取 sysfs，會 fallback 到所有 CPU [0, cpu_count)
  ///
  static std::vector<size_t> get_available_cpus() noexcept;

  // ----------------------------------------------------------------------------
  // 驗証
  // ----------------------------------------------------------------------------

  /// @brief 檢查 CPU ID 是否合法
  /// @param cpu_id CPU 編號
  /// @return true 如果 cpu_id 在範圍 [0, get_cpu_count()) 內
  ///
  static bool is_valid_cpu(size_t cpu_id) noexcept;

  /// @brief 檢查 CPU 是否可用
  /// @param cpu_id CPU 編號
  /// @return true 如果 CPU 是 online 且未被隔離
  ///
  static bool is_cpu_available(size_t cpu_id) noexcept;
};

}  // namespace  tx::sys
#endif
