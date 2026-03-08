#include "onyx/io/mapped_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <utility>

#include "onyx/error.hpp"
#include "onyx/io/file.hpp"

namespace onyx::io {

// ============================================================================
// MappedFile
// ============================================================================

// ----------------------------------------------------------------------------
// Private Constructor
// ----------------------------------------------------------------------------

MappedFile::MappedFile(File&& file, void* addr, size_t length, int prot, int flags,
                       off_t offset) noexcept
    : file_(std::move(file)),
      addr_(addr),
      length_(length),
      prot_(prot),
      flags_(flags),
      offset_(offset) {}

// ----------------------------------------------------------------------------
// Factory Methods
// ----------------------------------------------------------------------------

Result<MappedFile> MappedFile::from_file(File&& file, int prot, int flags, off_t offset,
                                         size_t length) {
  size_t map_length = length;

  if (map_length == 0) {
    size_t file_size = TRY(file.size());
    if (file_size <= static_cast<size_t>(offset)) {
      return onyx::fail(std::errc::invalid_argument, "Offset out of range");
    }
    map_length = file_size - static_cast<size_t>(offset);
  }

  void* addr = ::mmap(nullptr, map_length, prot, flags, file.fd(), offset);

  if (addr == MAP_FAILED) {
    return onyx::wrap_errno(errno, "mmap() failed");
  }

  return MappedFile(std::move(file), addr, map_length, prot, flags, offset);
}

Result<MappedFile> MappedFile::open_read(const fs::path& path) {
  File file = TRY(File::open(path, O_RDONLY));

  return from_file(std::move(file), PROT_READ, MAP_PRIVATE);
}

Result<MappedFile> MappedFile::open_write(const fs::path& path, size_t size) {
  File file = TRY(File::open(path, O_RDWR | O_CREAT));
  CHECK(file.resize(size));
  return from_file(std::move(file), PROT_READ | PROT_WRITE, MAP_SHARED);
}

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

MappedFile::~MappedFile() noexcept { unmap(); }

MappedFile::MappedFile(MappedFile&& other) noexcept
    : file_(std::move(other.file_)),
      addr_(std::exchange(other.addr_, nullptr)),
      length_(std::exchange(other.length_, 0)),
      prot_(other.prot_),
      flags_(other.flags_),
      offset_(other.offset_) {}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
  if (this != &other) {
    // clear current
    unmap();

    // move resources
    file_ = std::move(other.file_);
    addr_ = std::exchange(other.addr_, nullptr);
    length_ = std::exchange(other.length_, 0);
    prot_ = other.prot_;
    flags_ = other.flags_;
    offset_ = other.offset_;
  }
  return *this;
}

// ----------------------------------------------------------------------------
// Reader / Writer
// ----------------------------------------------------------------------------

MappedFile::Reader MappedFile::reader(size_t start_pos) const noexcept {
  return Reader(this, start_pos);
}

MappedFile::Appender MappedFile::into_appender(size_t start_pos) noexcept {
  return Appender(std::move(*this), start_pos);
}

// ----------------------------------------------------------------------------
// Accessors
// ----------------------------------------------------------------------------

std::optional<std::span<const std::byte>> MappedFile::slice(size_t offset,
                                                            size_t count) const noexcept {
  if (offset + count > length_) {
    return std::nullopt;
  }

  const std::byte* base = static_cast<const std::byte*>(addr_);
  return std::span<const std::byte>(base + offset, count);
}

// ----------------------------------------------------------------------------
// Management
// ----------------------------------------------------------------------------

Result<> MappedFile::resize(size_t new_size) noexcept {
  // 不用調整
  if (new_size == length_) return {};

  // Resize file
  CHECK(file_.resize(new_size));

  // 若一開始沒有映射，就幫他映射
  if (addr_ == nullptr && new_size > 0) {
    addr_ = ::mmap(nullptr, new_size, prot_, flags_, file_.fd(), offset_);
    if (addr_ == MAP_FAILED) {
      return onyx::fail(errno, "map() failed");
    };
    length_ = new_size;
    return {};
  }

  // 大小為零解除映射
  if (new_size == 0) {
    unmap();
    return {};
  }

  // mremap 可以直接將原本映射擴大/縮小 (性能好)
  // MREMAP_MAYMOVE:
  // 記憶體若可以直接進行擴充，無法直接分配可以搬移到全新一塊，
  void* new_addr = ::mremap(addr_, length_, new_size, MREMAP_MAYMOVE);

  if (new_addr == MAP_FAILED) {
    return onyx::fail(errno, "mremap() failed");
  }

  addr_ = new_addr;
  length_ = new_size;

  return {};
}

Result<> MappedFile::sync(bool async) noexcept {
  if (addr_ && length_ > 0) {
    if (::msync(addr_, length_, async ? MS_ASYNC : MS_SYNC) != 0) {
      return onyx::fail(errno, "msync() failed");
    }
  }
  return {};
}

Result<> MappedFile::lock() noexcept {
  if (addr_ && length_ > 0) {
    if (::mlock(addr_, length_) < 0) {
      return onyx::fail(errno, "mlock() failed");
    }
  }
  return {};
}

Result<> MappedFile::unlock() noexcept {
  if (addr_ && length_ > 0) {
    if (::munlock(addr_, length_) < 0) {
      return onyx::fail(errno, "munlock() failed");
    }
  }
  return {};
}

Result<> MappedFile::advise(int advice) noexcept {
  if (addr_ && length_ > 0) {
    if (::madvise(addr_, length_, advice) < 0) {
      return onyx::fail(errno, "madvice() failed");
    }
  }
  return {};
}

void MappedFile::prefault() noexcept {
  if (!addr_ || length_ == 0) return;

  volatile char* ptr = static_cast<char*>(addr_);

  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size < 0) {  // sysconf 失敗
    page_size = 4096;   // 採用默認的 4KB
  }

  // 遍歷每一頁進行讀取，觸發 OS 建立 Page Table Entry
  for (size_t i = 0; i < length_; i += static_cast<size_t>(page_size)) {
    (void)ptr[i];
  }

  // 確保讀取最後一頁
  if (length_ > 0) {
    (void)ptr[length_ - 1];
  }
}

// ----------------------------------------------------------------------------
// 手動管理
// ----------------------------------------------------------------------------

File MappedFile::into_inner() noexcept {
  unmap();
  return std::move(file_);
}

void MappedFile::unmap() noexcept {
  if (addr_ != nullptr) {
    ::munmap(addr_, length_);
    addr_ = nullptr;
    length_ = 0;
  }
}

// ============================================================================
// Reader
// ============================================================================

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

MappedFile::Reader::Reader(const MappedFile* mf, size_t start_pos) noexcept
    : mf_(mf), pos_(std::min(start_pos, mf->length_)) {}

// ----------------------------------------------------------------------------
// 讀取操作
// ----------------------------------------------------------------------------

std::span<const std::byte> MappedFile::Reader::read_bytes(size_t count) noexcept {
  size_t avail = std::min(count, remaining());
  const std::byte* ptr = mf_->raw_ptr() + pos_;
  pos_ += avail;
  return {ptr, avail};
}

std::span<const std::byte> MappedFile::Reader::read_until(std::byte delimiter) noexcept {
  if (eof()) {
    return {};
  }

  const std::byte* start = mf_->raw_ptr() + pos_;
  size_t max_len = remaining();

  // 搜索
  const std::byte* found =
      static_cast<const std::byte*>(std::memchr(start, static_cast<int>(delimiter), max_len));

  size_t len = 0;

  if (found != nullptr) {
    size_t diff = static_cast<size_t>(found - start);

    len = diff;
  } else {
    // 找不到，讀到文件末尾
    len = max_len;
  }

  pos_ += len;
  return std::span(start, len);
}

std::string_view MappedFile::Reader::read_line() noexcept {
  if (eof()) {
    return {};
  }

  const char* start = reinterpret_cast<const char*>(mf_->raw_ptr() + pos_);
  size_t max_len = remaining();

  // 搜索換行
  const char* found = static_cast<const char*>(std::memchr(start, '\n', max_len));

  size_t len = 0;
  size_t jump = 0;

  if (found != nullptr) {
    size_t diff = static_cast<size_t>(found - start);

    // 處理 \r\n 的情況
    bool is_crlf = (diff > 0 && start[diff - 1] == '\r');

    len = is_crlf ? diff - 1 : diff;  // 不包含 \n，且如果是 \r\n 則也不包含 \r

    jump = diff + 1;  // 跳過 \n
  } else {
    // 找不到換行符，讀到文件末尾
    len = max_len;
    jump = max_len;
  }

  pos_ += jump;
  return std::string_view(start, len);
}

// ============================================================================
// Appender
// ============================================================================

// ----------------------------------------------------------------------------
// Private Methods
// ----------------------------------------------------------------------------

Result<> MappedFile::Appender::ensure_capacity(size_t size) noexcept {
  size_t required = pos_ + size;

  if (required < mf_.length()) {
    return {};
  }

  // 1.5 倍擴容
  size_t new_cap = mf_.length() + (mf_.length() >> 1);

  // 至少滿足需求大小
  if (new_cap < required) {
    new_cap = required;
  }

  // 處理對齊，避免產生零碎空間
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size < 0) {
    page_size = 4096;  // 4KB
  }
  size_t rem = new_cap % static_cast<size_t>(page_size);
  if (rem != 0) {
    new_cap += (static_cast<size_t>(page_size) - rem);
  }

  CHECK(mf_.resize(new_cap));

  return {};
}

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

MappedFile::Appender::Appender(MappedFile&& mf, size_t start_pos) noexcept
    : mf_(std::move(mf)), pos_(start_pos) {
  // 若起始位置大於當前文件大小，先進行擴容
  if (pos_ > mf_.size()) {
    ensure_capacity(0);
  }
}

MappedFile::Appender::~Appender() noexcept {
  if (mf_.underlying_file().is_open()) {
    (void)mf_.resize(pos_);  // 忽略錯誤
  }
}

}  // namespace onyx::io
