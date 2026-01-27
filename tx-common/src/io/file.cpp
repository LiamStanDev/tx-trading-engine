#include "tx/io/file.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <system_error>

#include "tx/error.hpp"

namespace tx::io {

// ----------------------------------------------------------------------------
// Factory Methods
// ----------------------------------------------------------------------------

Result<File> File::open(std::string path, int flags, mode_t perm) {
  int fd = ::open(path.c_str(), flags, perm);

  if (fd < 0) {
    return tx::fail(errno);
  }

  return File(fd, std::move(path));
}

Result<File> File::create_temp(std::string template_path) noexcept {
  int fd = ::mkstemp(template_path.data());

  if (fd < 0) {
    return tx::fail(errno, "mkstemp() failed");
  }

  return File(fd, std::move(template_path));
}

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

File::~File() noexcept { close(); }

File& File::operator=(File&& other) noexcept {
  if (&other != this) {
    close();
    fd_ = std::exchange(other.fd_, -1);
    path_ = std::move(other.path_);
  }

  return *this;
}

// ----------------------------------------------------------------------------
// I/O 操作
// ----------------------------------------------------------------------------

Result<size_t> File::read(std::span<std::byte> buffer) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  ssize_t n;
  do {
    n = ::read(fd_, buffer.data(), buffer.size());
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno);
  }

  return static_cast<size_t>(n);
}

Result<size_t> File::write(std::span<const std::byte> data) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  ssize_t n;
  do {
    n = ::write(fd_, data.data(), data.size());
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno);
  }

  return static_cast<size_t>(n);
}
Result<size_t> File::pread(std::span<std::byte> buffer, off_t offset) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  if (offset < 0) {
    return tx::fail(std::errc::invalid_argument, "Invalid offset");
  }

  ssize_t n;
  do {
    n = ::pread(fd_, buffer.data(), buffer.size(), offset);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno, "pread() failed");
  }

  return static_cast<size_t>(n);
}

Result<size_t> File::pwrite(std::span<const std::byte> data,
                            off_t offset) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  if (offset < 0) {
    return tx::fail(std::errc::invalid_argument, "Invalid offset");
  }

  ssize_t n;
  do {
    n = ::pwrite(fd_, data.data(), data.size(), offset);
  } while (n < 0 && errno == EINTR);

  if (n < 0) {
    return tx::fail(errno, "pwrite() failed");
  }

  return static_cast<size_t>(n);
}

// ----------------------------------------------------------------------------
// Seek 操作
// ----------------------------------------------------------------------------

[[nodiscard]] Result<off_t> File::seek(off_t offset, Whence whence) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  off_t new_pos = ::lseek(fd_, offset, static_cast<int>(whence));

  if (new_pos < 0) {
    return tx::fail(errno);
  }

  return new_pos;
}

Result<off_t> File::tell() noexcept { return seek(0, Whence::Current); }

Result<> File::rewind() noexcept {
  CHECK(seek(0, Whence::Begin));

  return {};
}

// ----------------------------------------------------------------------------
// Sync 操作
// ----------------------------------------------------------------------------

Result<> File::sync() noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  if (::fsync(fd_) < 0) {
    return tx::fail(errno, "fsync() failed");
  }

  return {};
}

Result<> File::datasync() noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  if (::fdatasync(fd_) < 0) {
    return tx::fail(errno, "fdatasync() failed");
  }

  return {};
}

// ----------------------------------------------------------------------------
// Size 操作
// ----------------------------------------------------------------------------

[[nodiscard]] Result<size_t> File::size() const noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  struct stat st{};
  if (::fstat(fd_, &st) < 0) {
    return tx::fail(errno, "fstat() failed");
  }

  return static_cast<size_t>(st.st_size);
}

Result<> File::resize(size_t new_size) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  if (::ftruncate(fd_, static_cast<off_t>(new_size)) < 0) {
    return tx::fail(errno, "ftruncate() failed");
  }

  return {};
}

// ----------------------------------------------------------------------------
// Advice 操作
// ----------------------------------------------------------------------------

Result<> File::advise(Advise advise, off_t offset, size_t length) noexcept {
  if (!is_open()) {
    return tx::fail(std::errc::bad_file_descriptor);
  }

  // 直接返回錯誤碼
  int ret = ::posix_fadvise(fd_, offset, static_cast<off_t>(length),
                            static_cast<int>(advise));

  if (ret != 0) {  // 成功返回 0
    return tx::fail(ret, "posix_fadvise() failed");
  }

  return {};
}

// ----------------------------------------------------------------------------
// 手動管理
// ----------------------------------------------------------------------------

void File::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

/// @brief 釋放所有權
/// @return 原始 file descriptor
int File::release() noexcept {
  int fd = fd_;
  fd_ = -1;
  return fd;
}

}  // namespace tx::io
