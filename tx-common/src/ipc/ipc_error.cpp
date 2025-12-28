#include <fmt/format.h>

#include <tx/ipc/ipc_error.hpp>

namespace tx::ipc {

constexpr const char* to_string(IpcErrorCode code) noexcept {
  using enum IpcErrorCode;
  // 不能有 default 讓編譯器檢查是否有缺漏
  switch (code) {
    case ShmCreateFailed:
      return "ShmCreateFailed";
    case ShmOpenFailed:
      return "ShmOpenFailed";
    case ShmUnlinkFailed:
      return "ShmUnlinkFailed";
    case MmapFailed:
      return "MmapFailed";
    case MunmapFailed:
      return "MunmapFailed";
    case FtruncateFailed:
      return "FtruncateFailed";
    case FstatFailed:
      return "FstatFailed";
    case InvalidName:
      return "InvalidName";
    case InvalidSize:
      return "InvalidSize";
    case InvalidPermission:
      return "InvalidPermission";
    case AlreadyExists:
      return "AlreadyExists";
    case NotFound:
      return "NotFound";
    case PermissionDenied:
      return "PermissionDenied";
    case OutOfMemory:
      return "OutOfMemory";
    case TooManyFiles:
      return "TooManyFiles";
    case SystemError:
      return "SystemError";
    case InvalidParameter:
      return "InvalidParameter";
  }

  return "Unknowed";
}

const char* IpcError::code_name() const noexcept { return to_string(code_); }

IpcError IpcError::from_errno(IpcErrorCode code, std::string detail) noexcept {
  return IpcError(code, std::move(detail), errno);
}

std::string IpcError::message() const noexcept {
  // 如果有有效的 errno，包含系統錯誤訊息
  if (errno_ > 0) {
    return fmt::format("[{}]: {} (errno: {}, {})", code_name(), detail_, errno_,
                       std::strerror(errno_));
  }

  // 僅顯示錯誤碼與詳細訊息
  return fmt::format("[{}]: {}", code_name(), detail_);
}

}  // namespace tx::ipc
