#include "tx/io/mapped_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>

#include <utility>

#include "tx/error.hpp"
#include "tx/io/file.hpp"

namespace tx::io {

// ----------------------------------------------------------------------------
// Factory Methods
// ----------------------------------------------------------------------------

Result<MappedFile> MappedFile::from_file(File file, int prot, int flags,
                                         off_t offset, size_t length) {
  size_t file_size = TRY(file.size());

  // length = 0 表示整個文件
  size_t map_length = length == 0 ? file_size : length;

  if (offset < 0 || static_cast<size_t>(offset) + map_length > file_size) {
    return tx::fail(std::errc::invalid_argument, "Offset out of range");
  }

  void* addr = ::mmap(nullptr, map_length, prot, flags, file.fd(), offset);

  if (addr == MAP_FAILED) {
    return tx::fail(errno, "mmap() failed");
  }

  return MappedFile(std::move(file), addr, map_length);
}

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

MappedFile::MappedFile(File file, void* addr, size_t length) noexcept
    : file_(std::move(file)), addr_(addr), length_(length) {}

MappedFile::~MappedFile() noexcept { unmap(); }

MappedFile::MappedFile(MappedFile&& other) noexcept
    : file_(std::move(other.file_)),
      addr_(std::exchange(other.addr_, nullptr)),
      length_(std::exchange(other.length_, 0)) {};

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
  if (&other != this) {
    unmap();
    file_ = std::move(other.file_);
    addr_ = std::exchange(other.addr_, nullptr);
    length_ = std::exchange(other.length_, 0);
  }

  return *this;
}

// ----------------------------------------------------------------------------
// Access
// ----------------------------------------------------------------------------

std::optional<std::span<const std::byte>> MappedFile::slice(
    size_t offset, size_t count) const noexcept {
  if (offset + count > length_) {
    return std::nullopt;
  }

  const std::byte* base = static_cast<const std::byte*>(addr_);
  return std::span<const std::byte>(base + offset, count);
}

// ----------------------------------------------------------------------------
// Sync 操作
// ----------------------------------------------------------------------------

Result<> MappedFile::sync(int flags) noexcept {
  if (addr_ == nullptr) {
    return tx::fail(std::errc::bad_address, "MappedFile not mapped");
  }

  if (::msync(addr_, length_, flags) < 0) {
    return tx::fail(errno, "msync() failed");
  }

  return {};
}

// ----------------------------------------------------------------------------
// Advise 操作
// ----------------------------------------------------------------------------

Result<> MappedFile::advise(Advise advise) noexcept {
  if (addr_ == nullptr) {
    return tx::fail(std::errc::bad_address, "MappedFile not mapped");
  }

  if (::madvise(addr_, length_, static_cast<int>(advise)) < 0) {
    return tx::fail(errno, "madvise() failed");
  }

  return {};
}

// ----------------------------------------------------------------------------
// Accessor
// ----------------------------------------------------------------------------

File MappedFile::into_inner() noexcept {
  unmap();
  return std::move(file_);
}

// ----------------------------------------------------------------------------
// 手動管理
// ----------------------------------------------------------------------------

void MappedFile::unmap() noexcept {
  if (addr_ != nullptr) {
    ::munmap(addr_, length_);
    addr_ = nullptr;
    length_ = 0;
  }
}

}  // namespace tx::io
