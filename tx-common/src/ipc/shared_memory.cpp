#include "tx/ipc/shared_memory.hpp"

#include <fcntl.h>
#include <fmt/format.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>

#include "tx/core/result.hpp"
#include "tx/ipc/error.hpp"

namespace tx::ipc {

Result<SharedMemory> SharedMemory::create(std::string name, size_t size,
                                          mode_t mode) noexcept {
  if (name.empty() || name[0] != '/') {
    return Err(IpcError::from(IpcErrc::INVALID_SHM_NAME));
  }

  if (size == 0) {
    return Err(IpcError::from(IpcErrc::INVALID_SHM_SIZE));
  }

  // O_EXCL: 當已經存在會返回錯誤碼 (默認不會)，但我們必須知道是否是 owner
  // 所以得這樣做
  int fd = ::shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, mode);
  if (fd < 0) {
    return Err(IpcError::from(IpcErrc::SHM_EXISTED, errno));
  }

  if (::ftruncate(fd, static_cast<off_t>(size)) < 0) {
    int saved_errno = errno;
    ::close(fd);
    ::shm_unlink(name.c_str());

    return Err(IpcError::from(IpcErrc::SHM_CREATE_FAILED, saved_errno));
  }

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);
    ::shm_unlink(name.c_str());

    errno = saved_errno;
    return Err(IpcError::from(IpcErrc::SHM_CREATE_FAILED, saved_errno));
  }

  return Ok(SharedMemory(name, addr, size, fd, true));
}

Result<SharedMemory> SharedMemory::open(std::string name) noexcept {
  if (name.empty() || name[0] != '/') {
    return Err(IpcError::from(IpcErrc::INVALID_SHM_NAME));
  }

  int fd = ::shm_open(name.c_str(), O_RDWR, 0);
  if (fd < 0) {
    if (errno == ENOENT) {
      return Err(IpcError::from(IpcErrc::SHM_NOT_FOUND, errno));
    }
    if (errno == EACCES) {
      return Err(IpcError::from(IpcErrc::SHM_PERMISSION_DENY, errno));
    }
    return Err(IpcError::from(IpcErrc::SHM_OPEN_FAILED, errno));
  }

  struct stat file_stat;
  if (::fstat(fd, &file_stat) < 0) {
    int saved_errno = errno;
    ::close(fd);

    errno = saved_errno;
    return Err(IpcError::from(IpcErrc::SHM_OPEN_FAILED, saved_errno));
  }

  if (file_stat.st_size <= 0) {
    return Err(IpcError::from(IpcErrc::INVALID_SHM_SIZE));
  }

  size_t size = static_cast<size_t>(file_stat.st_size);

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);

    return Err(IpcError::from(IpcErrc::SHM_OPEN_FAILED, saved_errno));
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
