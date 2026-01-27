#include "tx/ipc/shared_memory.hpp"

#include <fcntl.h>
#include <fmt/format.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include "tx/error.hpp"

namespace tx::ipc {

// ----------------------------------------------------------------------------
// Factory Methods
// ----------------------------------------------------------------------------

Result<SharedMemory> SharedMemory::create(std::string name, size_t size,
                                          mode_t mode) noexcept {
  if (name.empty() || name[0] != '/') {
    return tx::fail(std::errc::invalid_argument, "SHM should start with '/'");
  }

  if (size == 0) {
    return tx::fail(std::errc::invalid_argument, "Invalid size");
  }

  // shm_open 預設會在 /dev/shm (tempfs) 下建立文件，不支援 hugepages
  int fd = ::shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, mode);

  if (fd < 0) {
    return tx::fail(errno, "shm_open() failed");
  }

  // truncate
  if (::ftruncate(fd, static_cast<off_t>(size)) < 0) {
    int saved_errno = errno;
    ::close(fd);
    ::shm_unlink(name.c_str());

    return tx::fail(saved_errno, "ftruncate() failed");
  }

  // mmap 操作是惰性操作，使用 MAP_POPULATE 預先分配內存，以避免延遲抖動。
  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);

    ::shm_unlink(name.c_str());
    return tx::fail(saved_errno, "mmap() failed");
  }

  return SharedMemory(name, addr, size, fd, true, false);
}

Result<SharedMemory> SharedMemory::create_huge(std::string name, size_t size,
                                               mode_t mode) noexcept {
  if (name.empty() || name[0] != '/') {
    return tx::fail(std::errc::invalid_argument, "SHM should start with '/'");
  }

  if (size == 0) {
    return tx::fail(std::errc::invalid_argument, "Invalid size");
  }

  std::string path = "/dev/hugepages" + name;
  int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, mode);

  if (fd < 0) {
    return tx::fail(errno, "open() failed");
  }

  // 大小對齊
  size_t actual_size = (size + kHugePageSize - 1) & ~(kHugePageSize - 1);

  // truncate
  if (::ftruncate(fd, static_cast<off_t>(actual_size)) < 0) {
    int saved_errno = errno;
    ::close(fd);
    ::unlink(path.c_str());

    return tx::fail(saved_errno, "ftruncate() failed");
  }

  // mmap 操作是惰性操作，使用 MAP_POPULATE 預先分配內存，以避免延遲抖動。
  // 注意：MAP_POPULATE 對 hugetlbfs 無效，需要手動預填充
  void* addr = ::mmap(nullptr, actual_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);

    ::unlink(path.c_str());

    return tx::fail(saved_errno, "mmap() failed");
  }

  // hugetlbfs 忽略 MAP_POPULATE，必須實際訪問記憶體才會分配
  std::memset(addr, 0, actual_size);

  return SharedMemory(path, addr, actual_size, fd, true, true);
}

Result<SharedMemory> SharedMemory::open(std::string name) noexcept {
  // 參數檢查
  if (name.empty() || name[0] != '/') {
    return tx::fail(std::errc::invalid_argument, "SHM should start with '/'");
  }

  int fd = ::shm_open(name.c_str(), O_RDWR, 0);

  if (fd < 0) {
    return tx::fail(errno, "shm_open() failed");
  }

  // 取得大小
  struct stat file_stat;
  if (::fstat(fd, &file_stat) < 0) {
    int saved_errno = errno;
    ::close(fd);

    return tx::fail(saved_errno, "fstat() failed");
  }

  if (file_stat.st_size <= 0) {
    return tx::fail(std::errc::invalid_argument, "Invalid size");
  }

  size_t size = static_cast<size_t>(file_stat.st_size);

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);

    return tx::fail(saved_errno, "mmap() failed");
  }

  return SharedMemory(name, addr, size, fd, false, false);
}

Result<SharedMemory> SharedMemory::open_huge(std::string name) noexcept {
  // 參數檢查
  if (name.empty() || name[0] != '/') {
    return tx::fail(std::errc::invalid_argument, "SHM should start with '/'");
  }

  std::string path = "/dev/hugepages" + name;
  int fd = ::open(path.c_str(), O_RDWR, 0);

  if (fd < 0) {
    return tx::fail(errno, "open() failed");
  }

  // 取得大小
  struct stat file_stat;
  if (::fstat(fd, &file_stat) < 0) {
    int saved_errno = errno;
    ::close(fd);

    return tx::fail(saved_errno, "fstat() failed");
  }

  if (file_stat.st_size <= 0) {
    return tx::fail(std::errc::invalid_argument, "Invalid size");
  }

  size_t size = static_cast<size_t>(file_stat.st_size);

  void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE, fd, 0);
  if (addr == MAP_FAILED) {
    int saved_errno = errno;
    ::close(fd);

    return tx::fail(saved_errno, "mmap() failed");
  }

  return SharedMemory(path, addr, size, fd, false, true);
}

void SharedMemory::release_resources() noexcept {
  if (addr_ != nullptr && addr_ != MAP_FAILED) {
    ::munmap(addr_, size_);
  }

  if (fd_ >= 0) {
    ::close(fd_);
  }

  if (owner_ && !name_.empty()) {
    if (huge_page_) {
      ::unlink(name_.c_str());
    } else {
      ::shm_unlink(name_.c_str());
    }
  }
}

SharedMemory::~SharedMemory() noexcept { release_resources(); }

SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept {
  if (this != &other) {
    // NOTE: 這邊不要調用解構函數，因為 name 也會被釋放掉，導致對已釋放的 name
    // 進行移動賦值 有危險 this->~SharedMemory();

    release_resources();

    name_ = std::move(other.name_);
    addr_ = std::exchange(other.addr_, nullptr);
    size_ = std::exchange(other.size_, 0);
    fd_ = std::exchange(other.fd_, -1);
    owner_ = std::exchange(other.owner_, false);
    huge_page_ = std::exchange(other.huge_page_, false);
  }

  return *this;
}

}  // namespace tx::ipc
