#ifndef TX_TRADING_ENGINE_IO_BUF_READER_HPP
#define TX_TRADING_ENGINE_IO_BUF_READER_HPP

#include <cstddef>
#include <vector>

#include "tx/error.hpp"
#include "tx/io/file.hpp"

namespace tx::io {

/// @brief 帶緩衝的檔案讀取器
///
/// 特性：
/// - 內部維護 buffer，減少 syscall 次數
/// - 支援 read_exact, read_until, read_line 等高階操作
///
class BufReader {
 private:
  File file_;                      ///< 擁有 File
  std::vector<std::byte> buffer_;  ///< 內部緩衝區
  size_t pos_{0};    ///< 當前讀取位置 (ptr + pos_: 當前該讀的位置)
  size_t valid_{0};  ///< buffer 中當前有效資料個數 (ptr + valid_: 末尾)

  explicit BufReader(File file, size_t capacity) noexcept;

 public:
  // ----------------------------------------------------------------------------
  // Constants
  // ----------------------------------------------------------------------------

  /// @brief 預設緩衝區大小64 KB
  inline static constexpr size_t kDefaultCapacity = 64 * 1024;

  // ----------------------------------------------------------------------------
  // Factory Methods
  // ----------------------------------------------------------------------------

  /// @brief 從現有 File 建立
  /// @param file File 物件（會被 move）
  /// @param capacity buffer 大小（預設 64KB）
  /// @return FileReader 或錯誤
  ///
  [[nodiscard]] static Result<BufReader> from_file(
      File file, size_t capacity = kDefaultCapacity) noexcept;

  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------
  ~BufReader() noexcept = default;
  BufReader(const BufReader&) = delete;
  BufReader& operator=(const BufReader&) = delete;
  BufReader(BufReader&&) noexcept = default;
  BufReader& operator=(BufReader&&) noexcept = default;

  // ----------------------------------------------------------------------------
  // Buffered Operations
  // ----------------------------------------------------------------------------

  /// @brief 讀取資料到 dest
  /// @param dest 目標 buffer
  /// @return 實際讀取的 bytes 數或錯誤
  ///
  [[nodiscard]] Result<size_t> read(std::span<std::byte> dest) noexcept;

  /// @brief 讀取恰好 n bytes
  /// @param dest 目標 buffer
  /// @return 成功或錯誤（EOF 也視為錯誤）
  /// @note 保證 EOF 或錯誤之前，讀滿整個 dest
  ///
  [[nodiscard]] Result<> read_exact(std::span<std::byte> dest) noexcept;

  /// @brief 讀取直到遇到 delimiter
  /// @param delimiter 分隔符號（例如 '\n'）
  /// @return 包含 delimiter 的資料 或 錯誤
  /// @note delimiter 會包含在返回的資料中
  ///
  [[nodiscard]] Result<std::vector<std::byte>> read_until(
      std::byte delimiter) noexcept;

  /// @brief 讀取直到 EOF
  /// @return 所有剩餘資料 或 錯誤
  ///
  [[nodiscard]] Result<std::vector<std::byte>> read_to_end() noexcept;

  /// @brief 讀取一行（不包含換行符號）
  /// @return 一行文字 或 錯誤
  /// @note EOF 返回 std::unexpected
  /// @note 自動處理 \n 和 \r\n
  ///
  [[nodiscard]] Result<std::string> read_line() noexcept;

  /// @brief 讀取一行（追加到現有 string）
  /// @param buf 目標 string（不會被清空）
  /// @return 讀取的 bytes 數（包含換行符號）或錯誤
  /// @note 類似 Rust BufRead::read_line
  ///
  [[nodiscard]] Result<size_t> read_line_into(std::string& buf) noexcept;

  /// @brief 讀取所有行
  /// @return 所有行的 vector 或 錯誤
  ///
  [[nodiscard]] Result<std::vector<std::string>> read_lines() noexcept;

  // ----------------------------------------------------------------------------
  // Status
  // ----------------------------------------------------------------------------

  /// @brief 檢查是否已到達 EOF
  /// @return true 如果 EOF
  ///
  [[nodiscard]] Result<bool> is_eof() noexcept;

  /// @brief 取得 buffer 容量
  ///
  [[nodiscard]] size_t capacity() const noexcept { return buffer_.capacity(); }

  // ----------------------------------------------------------------------------
  // Accessor
  // ----------------------------------------------------------------------------

  /// @brief 取得底層 File（唯讀）
  ///
  [[nodiscard]] const File& underlying_file() const noexcept { return file_; }

  /// @brief 釋放 File 所有權
  /// @warning buffer 中未讀資料會丟失
  ///
  [[nodiscard]] File into_inner() noexcept { return std::move(file_); }

 private:
  /// @brief 取得 buffer 中未讀取的資料量
  ///
  [[nodiscard]] size_t buffered() const noexcept { return valid_ - pos_; }

  /// @brief 填充 buffer（內部使用）
  /// @return 填充的 bytes 數或錯誤
  ///
  Result<size_t> fill_buffer() noexcept;
};

}  // namespace tx::io

#endif
