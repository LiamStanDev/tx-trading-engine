#ifndef ONYX_IO_MAPPED_FILE_HPP
#define ONYX_IO_MAPPED_FILE_HPP

#include <sys/mman.h>
#include <sys/types.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
#include <type_traits>

#include "onyx/error.hpp"
#include "onyx/io/file.hpp"

namespace onyx::io {

namespace fs = std::filesystem;

// ============================================================================
// MappedFile
// ============================================================================

/// @brief 高效能記憶體映射檔案 (Memory Mapped File)
class MappedFile {
 public:
  class Reader;
  class Appender;

 private:
  File file_;            ///< 文件 (持有所有權)
  void* addr_{nullptr};  ///< 映射地址
  size_t length_{0};     ///< 映射長度

  // --- 映射參數 ---
  int prot_{PROT_READ};    ///< Protection
  int flags_{MAP_SHARED};  ///< Map flags
  off_t offset_{0};        ///< Offset

  MappedFile(File&& file, void* addr, size_t len, int prot, int flags, off_t off) noexcept;

 public:
  // ----------------------------------------------------------------------------
  // Factory Methods
  // ----------------------------------------------------------------------------

  /// @brief 從現有 File 建立映射
  ///
  /// @param file File 物件（會被 move）
  /// @param prot 保護模式 (PROT_READ | PROT_WRITE | PROT_EXEC)
  /// @param flags 映射標誌 (MAP_PRIVATE | MAP_SHARED | MAP_POPULATE)
  /// @param offset 檔案偏移（必須是 page size 倍數，預設 0）
  /// @param length 映射長度（0 = 整個檔案）
  /// @return MappedFile 或錯誤
  static Result<MappedFile> from_file(File&& file, int prot = PROT_READ, int flags = MAP_SHARED,
                                      off_t offset = 0, size_t length = 0);

  /// @brief 開啟檔案並唯讀映射
  static Result<MappedFile> open_read(const fs::path& path);

  /// @brief 開啟檔案 (若無則建立) 並可寫映射，指定大小
  ///
  /// @param path 文件路徑
  /// @param size 文件大小，文件大小會被修改成此大小
  static Result<MappedFile> open_write(const fs::path& path, size_t size);

  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------
  ~MappedFile() noexcept;
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;

  // ----------------------------------------------------------------------------
  // Reader / Writer
  // ----------------------------------------------------------------------------

  /// @brief 建立讀取器
  ///
  /// @param start_pos 起始讀取位置，預設為 0
  /// @param Reader 物件
  [[nodiscard]] Reader reader(size_t start_pos = 0) const noexcept;

  /// @brief 轉換為 Appender (轉移所有權)
  ///
  /// @param start_pos 起始寫入位置，預設為 0 (覆寫模式) 或 size() (追加模式)
  /// @return Appender 物件
  [[nodiscard]] Appender into_appender(size_t start_pos = 0) noexcept;

  // ----------------------------------------------------------------------------
  // Accessors
  // ----------------------------------------------------------------------------

  /// @brief 取得映射的記憶體區域 (唯讀)
  std::span<const std::byte> data() const noexcept {
    return std::span(static_cast<std::byte*>(addr_), length_);
  }

  /// @brief 取得映射的記憶體區域 (可寫)
  ///
  /// @warning 只有在 flag 為 PROT_WRITE 時候有效
  [[nodiscard]] std::span<std::byte> data_mut() noexcept { return std::span(raw_ptr(), length_); }

  /// @brief 使用 string_view 讀取
  [[nodiscard]] std::string_view as_str() const noexcept {
    return std::string_view(reinterpret_cast<const char*>(addr_), length_);
  }

  /// @brief 取得指定範圍的 span
  ///
  /// @param offset 起始偏移
  /// @param count bytes 數量
  /// @return span 或 nullopt（超出範圍）
  [[nodiscard]] std::optional<std::span<const std::byte>> slice(size_t offset,
                                                                size_t count) const noexcept;

  // ----------------------------------------------------------------------------
  // Management
  // ----------------------------------------------------------------------------

  /// @brief 調整映射檔案的大小
  ///
  /// @param new_size 檔案的新大小（以位元組為單位）
  /// @return 成功或錯誤碼。
  [[nodiscard]] Result<> resize(size_t new_size) noexcept;

  /// @brief 同步記憶體數據至物理磁碟。
  /// 將記憶體中已修改的數據（Dirty Pages）刷寫回儲存裝置，確保數據持久化。
  ///
  /// @param async 若為 true，則發起非同步同步請求後立即返回；若為
  /// false（預設），則會阻塞直到寫入完成。
  /// @return 成功或錯誤碼。
  [[nodiscard]] Result<> sync(bool async = false) noexcept;

  /// @brief 鎖定映射的記憶體分頁。
  ///
  /// 將映射區段鎖定在物理記憶體（RAM）中，防止作業系統將其置換（Swap）到虛擬記憶體。
  ///
  /// @return 成功或錯誤碼。
  [[nodiscard]] Result<> lock() noexcept;

  /// @brief 解除記憶體分頁鎖定。
  ///
  /// 允許作業系統根據記憶體壓力將此區段的分頁移至交換空間（Swap space）。
  ///
  /// @return Result<> 若成功解除鎖定回傳 Success。
  [[nodiscard]] Result<> unlock() noexcept;

  /// @brief 提供記憶體使用建議以優化效能。
  ///
  /// 預先告知核心（Kernel）關於此記憶體範圍的使用模式，以便核心進行預讀或快取優化。
  ///
  /// @param advice 建議類型，例如：
  /// - POSIX_MADV_SEQUENTIAL: 預期順序存取
  /// - POSIX_MADV_RANDOM: 預期隨機存取
  /// - POSIX_MADV_WILLNEED: 預期近期會被存取
  /// - 詳情請見 man 2 madvise (裡面有非常多)
  /// @return 成功或錯誤碼
  [[nodiscard]] Result<> advise(int advice) noexcept;

  /// @brief 預熱記憶體
  void prefault() noexcept;

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  /// @brief 取得大小
  [[nodiscard]] size_t size() const noexcept { return length_; }

  /// @brief 檢查是否為空
  [[nodiscard]] bool empty() const noexcept { return length_ == 0; }

  /// @brief 取得原始指標
  [[nodiscard]] const std::byte* raw_ptr() const noexcept {
    return static_cast<const std::byte*>(addr_);
  }

  /// @brief 取得原始指標
  [[nodiscard]] std::byte* raw_ptr() noexcept { return static_cast<std::byte*>(addr_); }

  /// @brief 是否當前映射與文件描述符處於可操作狀態
  [[nodiscard]] bool is_open() const noexcept { return file_.is_open() && addr_ != nullptr; }

  [[nodiscard]] size_t length() const noexcept { return length_; }

  /// @brief 取得底層 File（唯讀）
  [[nodiscard]] const File& underlying_file() const noexcept { return file_; }

  // ----------------------------------------------------------------------------
  // 手動管理
  // ----------------------------------------------------------------------------

  /// @brief 釋放 File 所有權（會 munmap，File 回到原始狀態）
  [[nodiscard]] File into_inner() noexcept;

  /// @brief 手動解除映射
  void unmap() noexcept;
};

// ============================================================================
// Reader
// ============================================================================

class MappedFile::Reader {
 private:
  const MappedFile* mf_;  ///< MappedFile 指標(不持有)
  size_t pos_{0};         ///< 當前讀取游標

 public:
  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------

  explicit Reader(const MappedFile* mf, size_t start_pos) noexcept;

  Reader(const Reader&) noexcept = default;
  Reader& operator=(const Reader&) noexcept = default;
  Reader(Reader&&) noexcept = default;
  Reader& operator=(Reader&&) noexcept = default;

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  /// @brief 剩餘未讀取位元數
  [[nodiscard]] size_t remaining() const noexcept { return mf_->length_ - pos_; }

  /// @brief 是否已經讀完
  [[nodiscard]] bool eof() const noexcept { return pos_ >= mf_->length_; }

  /// @brief 取得當前 cursor 位置
  [[nodiscard]] size_t position() const noexcept { return pos_; }

  // ----------------------------------------------------------------------------
  // Cursor 操做
  // ----------------------------------------------------------------------------

  /// @brief 移動 cursor 至指定位置
  void seek(size_t new_pos) noexcept { pos_ = std::min(new_pos, mf_->length_); }

  /// @brief 跳過 n 個位元
  void skip(size_t n) noexcept { seek(pos_ + n); }

  // ----------------------------------------------------------------------------
  // 讀取操作
  // ----------------------------------------------------------------------------

  /// @brief 取得 T 類型
  template <typename T>
    requires std::is_trivially_copyable_v<T>
  [[nodiscard]] std::optional<T> read() noexcept {
    if (remaining() < sizeof(T)) {
      return std::nullopt;
    }

    T val;
    std::memcpy(&val, mf_->raw_ptr() + pos_, sizeof(T));
    pos_ += sizeof(T);
    return val;
  }

  /// @brief 讀取指定數量的位元
  /// @note 若數量小於指定數量，則返回剩餘內容
  [[nodiscard]] std::span<const std::byte> read_bytes(size_t count) noexcept;

  /// @brief 讀到含指定位元數量的內容，若未找到返回剩餘內容
  [[nodiscard]] std::span<const std::byte> read_until(std::byte delimiter) noexcept;

  /// @brief 讀取一行
  [[nodiscard]] std::string_view read_line() noexcept;

  /// @brief 使用 string_view 讀取
  [[nodiscard]] std::string_view as_str() const noexcept {
    return std::string_view(reinterpret_cast<const char*>(mf_->raw_ptr() + pos_), mf_->length());
  }
};

// ============================================================================
// Appender
// ============================================================================

class MappedFile::Appender {
 private:
  MappedFile mf_;  // 持有 MappedFile 所有權
  size_t pos_{0};  ///< 當前寫入游標 (也是有效資料大小)

  /// @brief 確保有足夠空間添加指定大小，若不足則觸發擴容
  ///
  /// @param size 預計添加的大小
  /// @return 成功或錯誤碼
  Result<> ensure_capacity(size_t size) noexcept;

 public:
  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------

  /// @brief 建構 Appender 並接管 MappedFile
  explicit Appender(MappedFile&& mf, size_t start_pos) noexcept;

  /// @brief 解構函數，會自動 Trim 並關閉
  ~Appender() noexcept;
  Appender(const Appender&) = delete;
  Appender& operator=(const Appender&) = delete;
  Appender(Appender&&) = default;
  Appender& operator=(Appender&&) = default;

  // ----------------------------------------------------------------------------
  // Write operations
  // ----------------------------------------------------------------------------

  /// @brief 寫入數據
  ///
  /// @param data 待寫入數據
  /// @return 成功或錯誤碼
  Result<> append(std::span<const std::byte> data) noexcept;

  /// @brief 寫入 POD 類型
  ///
  /// @param val 待寫入物件
  /// @return 成功或錯誤碼
  template <typename T>
    requires std::is_trivially_copyable_v<T>
  Result<> append(const T& val) noexcept {
    if (pos_ + sizeof(T) > mf_.length_) [[unlikely]] {
      ensure_capacity(sizeof(T));
    }

    std::memcpy(mf_.addr_, &val, sizeof(T));
    pos_ += sizeof(T);
    return {};
  }

  // ----------------------------------------------------------------------------
  // Queries
  // ----------------------------------------------------------------------------

  /// @brief 取得當前寫入位置
  [[nodiscard]] size_t position() const noexcept { return pos_; }

  // ----------------------------------------------------------------------------
  // Management
  // ----------------------------------------------------------------------------

  /// @brief 預先分配空間，用於減少擴容次數
  ///
  /// @return 成功或錯誤碼
  Result<> reserve(size_t capacity);

  /// @brief 完成寫入，修剪檔案，並交還 MappedFile 所有權
  ///
  /// @return 成功返回 MappedFile 所有權，失敗返回錯誤碼
  Result<MappedFile> finish() noexcept;
};

}  // namespace onyx::io

#endif
