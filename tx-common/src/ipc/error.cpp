#include "tx/ipc/error.hpp"

#include <fmt/format.h>

namespace tx::ipc {

std::string IpcCategory::message(int e) const {
  using enum IpcErrc;
  switch (static_cast<IpcErrc>(e)) {
    case INVALID_SHM_NAME:
      return "Invalid shared memory name (must start with '/')";
    case INVALID_SHM_SIZE:
      return "Invalid shared memory size (must be > 0)";
    case SHM_EXISTED:
      return "Shared memory already exited";
    case SHM_CREATE_FAILED:
      return "Failed to create shared memory";
    case SHM_OPEN_FAILED:
      return "Failed to open shared memory";
    case SHM_NOT_FOUND:
      return "Shared memory not found";
    case SHM_PERMISSION_DENY:
      return "Permission deny to access shared memory";
  }

  return "Unknown IPC error";
}

const IpcCategory& category() noexcept {
  const static IpcCategory instance;
  return instance;
}

}  // namespace tx::ipc
