#ifndef TX_TRADING_ENGINE_IPC_SHARED_MEMORY_HPP
#define TX_TRADING_ENGINE_IPC_SHARED_MEMORY_HPP

#include <sys/mman.h>
#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "tx/ipc/error.hpp"

namespace tx::ipc {

template <typename T>
concept SharedMemoryCompatible =
    std::is_trivially_copyable_v<T> &&  // 沒有自定義拷貝/移動構造函數與析構函數
    std::is_standard_layout_v<T>;       // 保證 C 語言的記憶體布局，沒有 virtual

/// @brief POXIS 共享記憶體 RAII 封裝
/// @note 用於記憶體映射，不提供同步機制
class SharedMemory {
 private:
  std::string name_;     ///< SHM 名稱
  void* addr_{nullptr};  ///< mmap 返回的地址
  size_t size_{0};       ///< 映射大小
  int fd_{-1};           ///< SHM 的 file descriptor
  bool owner_{false};    ///< 是否為創建者

  explicit SharedMemory(std::string name, void* addr, size_t size, int fd,
                        bool owner) noexcept
      : name_(std::move(name)),
        addr_(addr),
        size_(size),
        fd_(fd),
        owner_(owner) {}

 public:
  // ===============================
  // Factory Methods
  // ===============================
  /// @brief 創建新的共享記憶體
  /// @param name SHM 名稱（必須以 / 開頭，如 "/tx_queue"）
  /// @param size 大小（bytes）
  /// @param mode 權限（預設 0600）
  /// @return SharedMemory 或錯誤
  static Result<SharedMemory> create(std::string name, size_t size,
                                     mode_t mode) noexcept;
  /// @brief 打開現有的共享記憶體
  /// @param name SHM 名稱
  /// @return SharedMemory 或錯誤
  static Result<SharedMemory> open(std::string name) noexcept;

  // ===============================
  // RAII 管理
  // ===============================
  ~SharedMemory() noexcept;
  SharedMemory(const SharedMemory&) = delete;
  SharedMemory& operator=(const SharedMemory&) = delete;
  SharedMemory(SharedMemory&& other) noexcept
      : name_(std::move(other.name_)),
        addr_(std::exchange(other.addr_, nullptr)),
        size_(std::exchange(other.size_, 0)),
        fd_(std::exchange(other.fd_, -1)),
        owner_(std::exchange(other.owner_, false)) {}
  SharedMemory& operator=(SharedMemory&& other) noexcept;

  // ===============================
  // 訪問方法
  // ===============================
  /// @brief 取得映射記憶體位置
  [[nodiscard]] void* data() noexcept { return addr_; }

  /// @brief 取得大小
  [[nodiscard]] size_t size() const noexcept { return size_; }

  /// @brief 取得名稱
  [[nodiscard]] const std::string& name() const noexcept { return name_; }

  /// @brief 型別安全的訪問
  /// @details 確保地址按 T 的對齊要求對齊（避免 Unaligned Access）
  ///  - 例如：int64_t 需要 8-byte 對齊
  template <typename T>
  [[nodiscard]] T* as() noexcept
    requires SharedMemoryCompatible<T>
  {
    // 指標有效
    assert(is_valid());
    // 大小檢查
    assert(sizeof(T) <= size_);
    // 確保首地址對齊類型大小，確保性能與安全性
    assert((reinterpret_cast<uintptr_t>(addr_) & (alignof(T) - 1)) == 0);
    return static_cast<T*>(addr_);
  }

  // ==========================
  // 查詢函數
  // ==========================
  /// @brief 檢查是否有效
  [[nodiscard]] bool is_valid() const noexcept {
    // 這邊不用檢查 fd_ 因為就算他被關閉了，mmap 映射仍然有效
    return addr_ != nullptr && addr_ != MAP_FAILED;
  }
};
}  // namespace tx::ipc

#endif
