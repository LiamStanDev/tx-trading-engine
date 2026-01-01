#include "tx/ipc/shared_memory.hpp"

#include <fcntl.h>
#include <fmt/format.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tx/core/result.hpp"

namespace tx::ipc {

Result<SharedMemory> SharedMemory::create(std::string name, size_t size,
                                          mode_t mode) noexcept {
  if (name.empty() || name[0] != '/') {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "SHM name must start with '/'"));
  }

  if (size == 0) {
    return Err(
        Error::from_errc(std::errc::invalid_argument, "SHM size cannot be 0"));
  }

  // O_EXCL: 當已經存在會返回錯誤碼 (默認不會)，但我們必須知道是否是 owner
  // 所以得這樣做
  int fd = ::shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, mode);
  if (fd < 0) {
    return Err(Error::from_errno(fmt::format("Name '{}'", name)));
  }

  if (::ftruncate(fd, static_cast<ssize_t>(size)) < 0) {
    int saved_errno = errno;
    ::close(fd);
    ::shm_unlink(name.c_str());

    errno = saved_errno;
    return Err(
        Error::from_errno(fmt::format("Failed to set SHM size to {}", size)));
  }

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);
    ::shm_unlink(name.c_str());

    errno = saved_errno;
    return Err(Error::from_errno(fmt::format("Failed to mmap SHM: {}", name)));
  }

  return Ok(SharedMemory(name, addr, size, fd, true));
}

Result<SharedMemory> SharedMemory::open(std::string name) noexcept {
  if (name.empty() || name[0] != '/') {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "SHM name must start with '/'"));
  }

  int fd = ::shm_open(name.c_str(), O_RDWR, 0);
  if (fd < 0) {
    return Err(Error::from_errno(fmt::format("Name '{}'", name)));
  }

  struct stat file_stat;
  if (::fstat(fd, &file_stat) < 0) {
    int saved_errno = errno;
    ::close(fd);

    errno = saved_errno;
    return Err(Error::from_errno(fmt::format("Failed to mmap SHM: {}", name)));
  }

  if (file_stat.st_size < 0) {
    return Err(Error::from_errc(std::errc::invalid_argument,
                                "File size is less then 0"));
  }

  size_t size = static_cast<size_t>(file_stat.st_size);

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);

    errno = saved_errno;
    return Err(Error::from_errno(fmt::format("Failed to mmap SHM: {}", name)));
  }

  return Ok(SharedMemory(name, addr, size, fd, false));
}

SharedMemory::~SharedMemory() noexcept {
  if (addr_ != nullptr && addr_ != MAP_FAILED) {
    ::munmap(addr_, size_);
  }

  if (fd_ >= 0) {
    ::close(fd_);
  }

  if (owner_ && !name_.empty()) {
    ::shm_unlink(name_.c_str());
  }
}

SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept {
  if (this != &other) {
    // @NOTE 這邊不要調用解構函數，因為 name 也會被釋放掉，導致對已釋放的 name
    // 進行移動賦值 有危險 this->~SharedMemory();

    // 手動釋放
    if (addr_ != nullptr && addr_ != MAP_FAILED) {
      ::munmap(addr_, size_);
    }

    if (fd_ >= 0) {
      ::close(fd_);
    }

    if (owner_ && !name_.empty()) {
      ::shm_unlink(name_.c_str());
    }

    name_ = std::move(other.name_);
    addr_ = std::exchange(other.addr_, nullptr);
    size_ = std::exchange(other.size_, 0);
    fd_ = std::exchange(other.fd_, -1);
    owner_ = std::exchange(other.owner_, false);
  }

  return *this;
}

}  // namespace tx::ipc
