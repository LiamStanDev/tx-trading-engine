#ifndef TX_TRADING_ENGINE_IO_MAPPED_FILE_HPP
#define TX_TRADING_ENGINE_IO_MAPPED_FILE_HPP

#include <sys/mman.h>
#include <sys/types.h>

#include <cstddef>
#include <optional>
#include <span>

#include "tx/error.hpp"
#include "tx/io/file.hpp"

namespace tx::io {

/// @brief Memory-mapped file (零拷貝檔案存取)
///
/// 生命週期：
/// - 擁有 File 物件，確保 fd 不會提前關閉
/// - RAII 自動 munmap
///
/// @note
/// - 使用 mmap 將檔案直接映射到記憶體
/// - 零拷貝: 不需要 read/write syscall
/// - 適合大檔案、隨機存取
///
class MappedFile {
 private:
  File file_;
  void* addr_{nullptr};
  size_t length_{0};

  MappedFile(File file, void* addr, size_t size) noexcept;

 public:
  // ----------------------------------------------------------------------------
  // Factory Methods
  // ----------------------------------------------------------------------------

  /// @brief 從現有 File 建立映射
  /// @param file File 物件（會被 move）
  /// @param prot 保護模式 (PROT_READ | PROT_WRITE | PROT_EXEC)
  /// @param flags 映射標誌 (MAP_PRIVATE | MAP_SHARED | MAP_POPULATE)
  /// @param offset 檔案偏移（必須是 page size 倍數，預設 0）
  /// @param length 映射長度（0 = 整個檔案）
  /// @return MappedFile 或錯誤
  static Result<MappedFile> from_file(File file, int prot = PROT_READ,
                                      int flags = MAP_SHARED, off_t offset = 0,
                                      size_t length = 0);

  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------
  ~MappedFile() noexcept;
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;

  // ----------------------------------------------------------------------------
  // Access
  // ----------------------------------------------------------------------------

  /// @brief 取得映射的記憶體區域 (唯讀)
  std::span<const std::byte> data() const noexcept {
    return std::span(static_cast<std::byte*>(addr_), length_);
  }

  /// @brief 取得映射的記憶體區域 (可寫)
  /// @warning 只有在 flag 為 PROT_WRITE 時候有效
  std::span<std::byte> data() noexcept {
    return std::span(static_cast<std::byte*>(addr_), length_);
  }

  /// @brief 取得指定範圍的 span
  /// @param offset 起始偏移
  /// @param count bytes 數量
  /// @return span 或 nullopt（超出範圍）
  [[nodiscard]] std::optional<std::span<const std::byte>> slice(
      size_t offset, size_t count) const noexcept;

  /// @brief 取得大小
  [[nodiscard]] size_t size() const noexcept { return length_; }

  /// @brief 檢查是否為空
  [[nodiscard]] bool empty() const noexcept { return length_ == 0; }

  // ----------------------------------------------------------------------------
  // Sync 操作
  // ----------------------------------------------------------------------------

  /// @brief 同步到磁碟
  /// @param flags MS_SYNC (同步) | MS_ASYNC (非同步) | MS_INVALIDATE
  /// @return 成功或錯誤
  [[nodiscard]] Result<> sync(int flags = MS_SYNC) noexcept;

  // ----------------------------------------------------------------------------
  // Advise 操作
  // ----------------------------------------------------------------------------

  /// @brief 提供記憶體存取建議 (madvise)
  enum class Advise {
    Normal = MADV_NORMAL,          ///< 預設
    Random = MADV_RANDOM,          ///< 隨機存取 (減少預讀)
    Sequential = MADV_SEQUENTIAL,  ///< 順序讀取 (預讀更多)
    WillNeed = MADV_WILLNEED,      ///< 即將需要 (預載入)
    DontNeed = MADV_DONTNEED,      ///< 不需要 (立即丟棄)
  };

  /// @brief 提供存取建議
  /// @param advise 存取模式建議
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> advise(Advise advise) noexcept;

  // ----------------------------------------------------------------------------
  // Accessor
  // ----------------------------------------------------------------------------

  /// @brief 取得底層 File（唯讀）
  [[nodiscard]] const File& underlying_file() const noexcept { return file_; }

  /// @brief 釋放 File 所有權（會 munmap，File 回到原始狀態）
  [[nodiscard]] File into_inner() noexcept;

  // ----------------------------------------------------------------------------
  // 手動管理
  // ----------------------------------------------------------------------------

  /// @brief 手動解除映射
  void unmap() noexcept;
};

}  // namespace tx::io

#endif
