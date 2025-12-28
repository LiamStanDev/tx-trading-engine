#include <fmt/format.h>

#include <tx/network/socket_error.hpp>

namespace tx::network {

SocketError SocketError::from_errno(SocketErrorCode code,
                                    std::string detail) noexcept {
  return SocketError{code, std::move(detail), errno};
}

constexpr const char* to_string(SocketErrorCode code) noexcept {
  using enum SocketErrorCode;

  switch (code) {
    // Socket 生命週期
    case CreateFailed:
      return "CreateFailed";
    case BindFailed:
      return "BindFailed";
    case ListenFailed:
      return "ListenFailed";
    case AcceptFailed:
      return "AcceptFailed";
    case ConnectFailed:
      return "ConnectFailed";

    // I/O 操作
    case SendFailed:
      return "SendFailed";
    case RecvFailed:
      return "RecvFailed";
    case WouldBlock:
      return "WouldBlock";

    // Socket 選項設定
    case SetSockOptFailed:
      return "SetSockOptFailed";
    case GetSockOptFailed:
      return "GetSockOptFailed";
    case FcntlGetFailed:
      return "FcntlGetFailed";
    case FcntlSetFailed:
      return "FcntlSetFailed";

    // Multicast 相關
    case JoinMulticastFailed:
      return "JoinMulticastFailed";
    case LeaveMulticastFailed:
      return "LeaveMulticastFailed";

    // 查詢操作
    case GetSockNameFailed:
      return "GetSockNameFailed";
    case GetPeerNameFailed:
      return "GetPeerNameFailed";

    // 狀態錯誤
    case InvalidState:
      return "InvalidState";
    case InvalidParameter:
      return "InvalidParameter";
    case ConnectionClosed:
      return "ConnectionClosed";
    case AlreadyClosed:
      return "AlreadyClosed";
  }

  return "Unknowed";
}

const char* SocketError::code_name() const noexcept { return to_string(code_); }

std::string SocketError::message() const noexcept {
  if (errno_ > 0) {
    return fmt::format("[{}]: {} (errno: {}, {})", code_name(), detail_, errno_,
                       std::strerror(errno_));
  }
  return fmt::format("[{}]: {}", code_name(), detail_);
}

}  // namespace tx::network
