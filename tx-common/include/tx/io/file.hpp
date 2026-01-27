#ifndef TX_TRADING_ENGINE_IO_FILE_HPP
#define TX_TRADING_ENGINE_IO_FILE_HPP

#include <fcntl.h>

#include <cstdio>
#include <span>
#include <string>
#include <utility>

#include "tx/error.hpp"

namespace tx::io {

// ----------------------------------------------------------------------------
// File
// ----------------------------------------------------------------------------

/// @brief 高效能檔案 I/O 類別
///
/// - RAII 管理 file descriptor
/// - 直接操作 POSIX API
///
class File {
 private:
  int fd_{-1};        ///< file descriptor
  std::string path_;  ///< 文件路徑

  explicit File(int fd, std::string path) noexcept
      : fd_(fd), path_(std::move(path)) {}

 public:
  // ----------------------------------------------------------------------------
  // Factory Methods
  // ----------------------------------------------------------------------------

  /// @brief 開啟檔案
  /// @param path 檔案路徑
  /// @param flags 開啟模式 (O_RDONLY, O_RDWR, O_CREAT, ...)
  /// @param perm 權限 (僅用於 Create 模式下)
  /// @return File 或者錯誤
  ///
  [[nodiscard]] static Result<File> open(std::string path, int flags,
                                         mode_t perm = 0644);

  /// @brief 建立臨時檔案
  /// @param template_path 模板路徑 (例如: "/tmp/myfile.XXXXXX")
  /// @return File 物件或錯誤碼
  [[nodiscard]] static Result<File> create_temp(
      std::string template_path) noexcept;

  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------

  ~File() noexcept;
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&& other) noexcept
      : fd_(std::exchange(other.fd_, -1)), path_(std::move(other.path_)) {}
  File& operator=(File&& other) noexcept;

  // ----------------------------------------------------------------------------
  // I/O 操作
  // ----------------------------------------------------------------------------

  /// @brief 讀取資料
  /// @param buffer 目標 buffer
  /// @return 實際讀取的 bytes 數或者錯誤
  ///
  [[nodiscard]] Result<size_t> read(std::span<std::byte> buffer) noexcept;

  /// @brief 寫入資料
  /// @param data 要寫入的資料
  /// @return 實際寫入的 bytes 數或者錯誤
  ///
  [[nodiscard]] Result<size_t> write(std::span<const std::byte> data) noexcept;

  /// @brief 定位讀取 (Positional read)
  /// @param buffer 目標 buffer
  /// @param offset 偏移量
  /// @return 實際讀取的 bytes 數或錯誤碼
  /// @note 不改變 file position
  ///
  [[nodiscard]] Result<size_t> pread(std::span<std::byte> buffer,
                                     off_t offset) noexcept;

  /// @brief 定位寫入 (Positional write)
  /// @param data 要寫入的資料
  /// @param offset 偏移量
  /// @return 實際寫入的 bytes 數或錯誤碼
  /// @note 不改變 file position
  ///
  [[nodiscard]] Result<size_t> pwrite(std::span<const std::byte> data,
                                      off_t offset) noexcept;

  // ----------------------------------------------------------------------------
  // Seek 操作
  // ----------------------------------------------------------------------------

  /// @brief 起始位置
  enum class Whence {
    Begin = SEEK_SET,    ///< 從檔案開頭
    Current = SEEK_CUR,  ///< 從當前位置
    End = SEEK_END,      ///< 從檔案結尾
  };

  /// @brief 移動檔案位置
  /// @param offset 偏移量
  /// @param whence 起始位置 (SEEK_SET: 開頭, SEE_CUR: 當前, SEE_END: 結尾)
  /// @return 新的絕對位置或錯誤碼
  [[nodiscard]] Result<off_t> seek(off_t offset,
                                   Whence whence = Whence::Begin) noexcept;

  /// @brief 取得當前檔案位置
  /// @return 當前位置或錯誤碼
  [[nodiscard]] Result<off_t> tell() noexcept;

  /// @brief 回到檔案開頭
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> rewind() noexcept;

  // ----------------------------------------------------------------------------
  // Sync 操作
  // ----------------------------------------------------------------------------

  /// @brief 同步資料到磁碟 (包含 metadata)
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> sync() noexcept;

  /// @brief 只同步資料 (不同步 metadata)
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> datasync() noexcept;

  // ----------------------------------------------------------------------------
  // Size 操作
  // ----------------------------------------------------------------------------

  /// @brief 取得檔案大小
  /// @return 檔案大小 (bytes) 或錯誤碼
  [[nodiscard]] Result<size_t> size() const noexcept;

  /// @brief 調整檔案大小
  /// @param new_size 新大小
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> resize(size_t new_size) noexcept;

  // ----------------------------------------------------------------------------
  // Advise 操作
  // ----------------------------------------------------------------------------

  /// @brief 提示內核檔案存取模式 (fadvise)
  enum class Advise {
    Normal = POSIX_FADV_NORMAL,          ///< 預設
    Sequential = POSIX_FADV_SEQUENTIAL,  ///< 順序讀取 (預讀更多)
    Random = POSIX_FADV_RANDOM,          ///< 隨機存取 (減少預讀)
    NoReuse = POSIX_FADV_NOREUSE,        ///< 只用一次 (立即丟棄 cache)
    WillNeed = POSIX_FADV_WILLNEED,      ///< 即將需要 (預載入)
    DontNeed = POSIX_FADV_DONTNEED,      ///< 不需要 (立即丟棄)
  };

  /// @brief 提供存取建議
  /// @param advise 存取模式建議
  /// @param offset 起始偏移
  /// @param length 長度 (0 = 整個檔案)
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> advise(Advise advise, off_t offset = 0,
                                size_t length = 0) noexcept;

  // ----------------------------------------------------------------------------
  // Access
  // ----------------------------------------------------------------------------

  /// @brief 檢查檔案是否開啟
  [[nodiscard]] bool is_open() const noexcept { return fd_ >= 0; }

  /// @brief 取得原始 file descriptor
  [[nodiscard]] int fd() const noexcept { return fd_; }

  /// @brief 取得檔案路徑
  [[nodiscard]] const std::string& path() const noexcept { return path_; }

  // ----------------------------------------------------------------------------
  // 手動管理
  // ----------------------------------------------------------------------------

  /// @brief 關閉檔案
  void close() noexcept;

  /// @brief 釋放所有權
  /// @return 原始 file descriptor
  int release() noexcept;
};

}  // namespace tx::io

#endif
