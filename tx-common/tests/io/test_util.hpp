#ifndef TX_COMMON_TESTS_IO_TEST_UTIL_HPP
#define TX_COMMON_TESTS_IO_TEST_UTIL_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <random>
#include <vector>

// ----------------------------------------------------------------------------
// Data Generator
// ----------------------------------------------------------------------------

namespace tx::io::test {

// ----------------------------------------------------------------------------
// Temporary File
// ----------------------------------------------------------------------------

/// @brief RAII 管理臨時檔案
/// @brief RAII 管理臨時檔案（無異常版本）
class TempFile {
 private:
  std::string path_;
  bool valid_{false};

 public:
  /// @brief 建立臨時檔案
  /// @note 建構失敗時，is_valid() 返回 false
  TempFile() {
    char tmpl[] = "/tmp/tx-test-XXXXXX";
    int fd = ::mkstemp(tmpl);
    if (fd >= 0) {
      ::close(fd);
      path_ = tmpl;
      valid_ = true;
    }
  }

  /// @brief 解構時自動刪除檔案
  ~TempFile() {
    if (valid_ && !path_.empty()) {
      ::unlink(path_.c_str());
    }
  }

  // Non-copyable, movable
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;

  TempFile(TempFile&& other) noexcept
      : path_(std::move(other.path_)), valid_(other.valid_) {
    other.valid_ = false;
    other.path_.clear();
  }

  TempFile& operator=(TempFile&& other) noexcept {
    if (this != &other) {
      if (valid_ && !path_.empty()) {
        ::unlink(path_.c_str());
      }
      path_ = std::move(other.path_);
      valid_ = other.valid_;
      other.valid_ = false;
      other.path_.clear();
    }
    return *this;
  }

  /// @brief 檢查臨時檔案是否成功建立
  [[nodiscard]] bool is_valid() const noexcept { return valid_; }

  /// @brief 取得檔案路徑
  [[nodiscard]] const std::string& path() const noexcept { return path_; }

  /// @brief 寫入字串內容到檔案
  /// @param content 要寫入的內容
  /// @return true 如果成功
  bool write_content(std::string_view content) noexcept {
    if (!valid_) return false;

    int fd = ::open(path_.c_str(), O_WRONLY | O_TRUNC);
    if (fd < 0) return false;

    ssize_t n = ::write(fd, content.data(), content.size());
    ::close(fd);

    return n == static_cast<ssize_t>(content.size());
  }

  /// @brief 寫入 bytes 到檔案
  /// @param data 要寫入的資料
  /// @return true 如果成功
  bool write_bytes(const std::vector<std::byte>& data) noexcept {
    if (!valid_) return false;

    int fd = ::open(path_.c_str(), O_WRONLY | O_TRUNC);
    if (fd < 0) return false;

    ssize_t n = ::write(fd, data.data(), data.size());
    ::close(fd);

    return n == static_cast<ssize_t>(data.size());
  }

  /// @brief 讀取檔案內容為字串
  /// @return 檔案內容，失敗返回 nullopt
  [[nodiscard]] std::optional<std::string> read_content() const noexcept {
    if (!valid_) return std::nullopt;

    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0) return std::nullopt;

    struct stat st;
    if (::fstat(fd, &st) < 0) {
      ::close(fd);
      return std::nullopt;
    }

    std::string content(st.st_size, '\0');
    ssize_t n = ::read(fd, content.data(), st.st_size);
    ::close(fd);

    if (n != st.st_size) return std::nullopt;

    return content;
  }

  /// @brief 讀取檔案內容為 bytes
  /// @return bytes 資料，失敗返回 nullopt
  [[nodiscard]] std::optional<std::vector<std::byte>> read_bytes()
      const noexcept {
    if (!valid_) return std::nullopt;

    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0) return std::nullopt;

    struct stat st;
    if (::fstat(fd, &st) < 0) {
      ::close(fd);
      return std::nullopt;
    }

    std::vector<std::byte> data(st.st_size);
    ssize_t n = ::read(fd, data.data(), st.st_size);
    ::close(fd);

    if (n != st.st_size) return std::nullopt;

    return data;
  }
};

inline std::vector<std::byte> random_bytes(size_t n) {
  /// rd 其實可以做為 gen 的功能，但是每次呼叫就需要 syscall
  /// 所以使用 mt199739 偽隨機算法性能更好
  std::random_device rd;   // (真)亂數種子，讀取作業系統底層雜訊(熱訊號)
  std::mt19937 gen(rd());  // (偽)隨機生成引擎，透過數學公式產生一系列隨機數列
  std::uniform_int_distribution<int> dis(0, 255);  // 切割成為分配

  std::vector<std::byte> data(n);
  for (auto& b : data) {
    b = static_cast<std::byte>(dis(gen));
  }

  return data;
}

}  // namespace tx::io::test
#endif
