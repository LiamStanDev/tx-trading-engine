#include "tx/ipc/error.hpp"

#include <fmt/format.h>

#include <system_error>

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

const IpcCategory& ipc_category() noexcept {
  const static IpcCategory instance;
  return instance;
}

std::error_code make_error_code(IpcErrc ec) noexcept {
  return std::error_code(static_cast<int>(ec), ipc_category());
}

IpcError IpcError::from(IpcErrc errc) noexcept {
  return IpcError(make_error_code(errc));
}

IpcError IpcError::from(IpcErrc errc, int e) noexcept {
  return IpcError(make_error_code(errc), e);
}

std::string IpcError::message() const noexcept {
  std::string header = fmt::format("[{}:{}]: {}", code_.category().name(),
                                   code_.value(), code_.message());

  if (errno_ == 0) {
    return header;
  } else {
    return fmt::format("{}\n └─▶ errno({}): {}", header, errno_,
                       std::generic_category().message(errno_));
  }
}

}  // namespace tx::ipc
