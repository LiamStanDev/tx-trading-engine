#ifndef ONYX_IO_FILE_HPP
#define ONYX_IO_FILE_HPP

#include <fcntl.h>
#include <sys/stat.h>

#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "onyx/error.hpp"

namespace onyx::io {

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// File
// ----------------------------------------------------------------------------

/// @brief 文件 RAII 封裝並提供更多文件常見操作
class File {
 private:
  int fd_{-1};     ///< file descriptor
  fs::path path_;  ///< file path

  explicit File(int fd, const fs::path& path) noexcept : fd_(fd), path_(path) {}

 public:
  // ----------------------------------------------------------------------------
  // Factory Methods
  // ----------------------------------------------------------------------------

  /// @brief 開啟文件
  ///
  /// @param flags POSIX 標記如：O_RDONLY, O_RDWR, O_DIRECT，請見 man 2 open
  [[nodiscard]] static Result<File> open(const fs::path& path, int flags, mode_t perm = 0644);

  /// @brief 建立臨時檔案
  ///
  /// @param template_path 模板路徑 (例如:
  /// "/tmp/myfile.XXXXXX")，在執行完之後會變成實際名稱
  /// @return File 物件或錯誤碼
  [[nodiscard]] static Result<File> create_temp(std::string& template_path) noexcept;

  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------

  ~File() noexcept;
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&& other) noexcept : fd_(std::exchange(other.fd_, -1)), path_(std::move(other.path_)) {}
  File& operator=(File&& other) noexcept;

  // ----------------------------------------------------------------------------
  // I/O Operations
  // ----------------------------------------------------------------------------

  /// @brief 讀取文件內容
  ///
  /// @param buffer 讀取緩衝區
  /// @return 實際讀取的位元數
  [[nodiscard]] Result<size_t> read(std::span<std::byte> buffer) noexcept;

  /// @brief 寫入文件
  ///
  /// @param data 寫入緩衝區
  /// @return 實際寫入的位元數
  [[nodiscard]] Result<size_t> write(std::span<const std::byte> data) noexcept;

  /// @brief 指定位置讀取文件，不依賴於內部偏移量，多執行續安全
  ///
  /// @param buffer 讀取緩衝區
  /// @return 實際讀取的位元數
  [[nodiscard]] Result<size_t> pread(std::span<std::byte> buffer, off_t offset) noexcept;

  /// @brief 指定位置寫入文件，不依賴於內部偏移量，多執行續安全
  ///
  /// @param data 寫入緩衝區
  /// @return 實際寫入的位元數
  [[nodiscard]] Result<size_t> pwrite(std::span<const std::byte> data, off_t offset) noexcept;

  // ----------------------------------------------------------------------------
  // Convenience Operations (Zero-Copy Optimized)
  // ----------------------------------------------------------------------------

  /// @brief 精確讀取指定位元數
  ///
  /// @return 成功: 當讀取指定位元數；失敗: 未讀取到指定位元數（含提前 EOF）
  [[nodiscard]] Result<> read_exact(std::span<std::byte> buffer) noexcept;

  /// @brief 讀取整個文件內容
  ///
  /// @return 成功：返回完整文件內容；失敗：返回錯誤碼
  [[nodiscard]] Result<std::vector<std::byte>> read_to_end() noexcept;

  /// @brief 讀取整個文件使用字符串
  ///
  /// @return 成功：返回完整文件內容；失敗：返回錯誤碼
  [[nodiscard]] Result<std::string> read_to_string() noexcept;

  // ----------------------------------------------------------------------------
  // Seek Operations
  // ----------------------------------------------------------------------------

  enum class Whence {
    Begin = SEEK_SET,
    Current = SEEK_CUR,
    End = SEEK_END,
  };

  [[nodiscard]] Result<off_t> seek(off_t offset, Whence whence = Whence::Begin) noexcept;
  [[nodiscard]] Result<off_t> tell() noexcept;
  [[nodiscard]] Result<> rewind() noexcept;

  // ----------------------------------------------------------------------------
  // Sync Operations
  // ----------------------------------------------------------------------------

  [[nodiscard]] Result<> sync() noexcept;
  [[nodiscard]] Result<> datasync() noexcept;

  // ----------------------------------------------------------------------------
  // Size Operations
  // ----------------------------------------------------------------------------

  [[nodiscard]] Result<size_t> size() const noexcept;
  [[nodiscard]] Result<> resize(size_t new_size) noexcept;

  // ----------------------------------------------------------------------------
  // Advise Operations
  // ----------------------------------------------------------------------------

  /// @brief 文件建議
  ///
  /// 詳情請見 man 2 posix_fadvise
  enum class Advise {
    /// @brief 預設行為 (Default)，系統自行猜測存取模式，無特殊優化。
    Normal = POSIX_FADV_NORMAL,

    /// @brief 預期循序存取 (Sequential Access)，告訴 Kernel 加大 Read-Ahead
    /// (預讀) 視窗。
    ///
    /// @note 適用場景：讀取大型日誌檔、回放歷史數據、備份。
    Sequential = POSIX_FADV_SEQUENTIAL,

    /// @brief 預期隨機存取 (Random Access)，告訴 Kernel 關閉 Read-Ahead
    /// 機制，避免預讀無用資料浪費 I/O。
    ///
    /// @note 適用場景：資料庫查詢、索引查找、跳躍讀取二進制檔。
    Random = POSIX_FADV_RANDOM,

    /// @brief 近期將會使用 (Pre-fetch)，觸發非阻塞 (Non-blocking)
    /// 的背景讀取，將資料預先載入 Page Cache。
    ///
    /// @note 適用場景：啟動時預載熱數據 (Hot Data)。
    WillNeed = POSIX_FADV_WILLNEED,

    /// @brief 近期不再使用 (Drop Cache)，建議 Kernel 釋放這段資料的 Page
    /// Cache。
    ///
    /// @note
    /// - 必須確保資料已寫入磁碟 (Clean Pages)，若為 Dirty Pages 則無效
    /// (需先 fsync)。
    /// - 適用場景：備份完大檔案後釋放記憶體、避免一次性讀取的大檔沖掉熱數據。
    DontNeed = POSIX_FADV_DONTNEED,

    /// @brief 資料只讀一次 (Streaming)，告訴 Page Replace
    /// 演算法讀完即丟，不列入LRU 計算。
    ///
    /// @note
    /// - Linux 6.3+ 才真正有效。
    /// - 適用場景：串流媒體、處理完即丟棄的超大數據流。
    NoReuse = POSIX_FADV_NOREUSE,
  };

  /// @brief 提供文件使用建議以優化效能。
  /// 預先告知核心（Kernel）關於此文件的存取模式，以便核心進行預讀或快取優化。
  ///
  /// @param advice 建議類型
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> advise(Advise advise, off_t offset = 0, size_t length = 0) noexcept;

  // ----------------------------------------------------------------------------
  // Access
  // ----------------------------------------------------------------------------

  [[nodiscard]] bool is_open() const noexcept { return fd_ >= 0; }
  [[nodiscard]] int fd() const noexcept { return fd_; }
  [[nodiscard]] const fs::path& path() const noexcept { return path_; }

  // ----------------------------------------------------------------------------
  // Manual Management
  // ----------------------------------------------------------------------------

  /// @brief 關閉文件
  void close() noexcept;

  /// @brief 釋放文件所有權，不關閉文件
  ///
  /// @return 文件描述符
  /// @warning 需要將返回的文件描述符手動關閉
  int release() noexcept;
};

}  // namespace onyx::io

#endif
