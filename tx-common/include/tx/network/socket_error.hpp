#ifndef TX_TRADING_ENGINE_NETWORK_SOCKET_ERROR_HPP
#define TX_TRADING_ENGINE_NETWORK_SOCKET_ERROR_HPP

#include <cstdint>
#include <string>
#include <tx/core/result.hpp>

namespace tx::network {

enum class SocketErrorCode : uint8_t {
  // ===========================
  // Socket 生命週期
  // ===========================
  CreateFailed,   // socket() 失敗
  BindFailed,     // bind() 失敗
  ListenFailed,   // listen() 失敗
  AcceptFailed,   // accept() 失敗
  ConnectFailed,  // connect() 失敗

  // ===========================
  // I/O 操作
  // ===========================
  SendFailed,  // send()/sendto() 失敗
  RecvFailed,  // recv()/recvfrom() 失敗
  WouldBlock,  // EAGAIN/EWOULDBLOCK（非阻塞模式下的正常狀態）

  // ===========================
  // Socket 選項設定
  // ===========================
  SetSockOptFailed,  // setsockopt() 失敗
  GetSockOptFailed,  // getsockopt() 失敗
  FcntlGetFailed,    // fcntl(F_GETFL) 失敗
  FcntlSetFailed,    // fcntl(F_SETFL) 失敗

  // ===========================
  // Multicast 相關
  // ===========================
  JoinMulticastFailed,   // IP_ADD_MEMBERSHIP 失敗
  LeaveMulticastFailed,  // IP_DROP_MEMBERSHIP 失敗

  // ===========================
  // 查詢操作
  // ===========================
  GetSockNameFailed,  // getsockname() 失敗（查詢本地地址）
  GetPeerNameFailed,  // getpeername() 失敗（查詢遠端地址）

  // ===========================
  // 狀態錯誤
  // ===========================
  InvalidState,      // Socket 狀態無效（如 fd < 0）
  InvalidParameter,  // 參數驗證失敗（如 TTL 超出範圍）
  ConnectionClosed,  // 連線已關閉（對端發送 FIN 或 RST）
  AlreadyClosed,     // Socket 已被本地關閉
};

class SocketError {
 private:
  SocketErrorCode code_;
  int errno_;
  std::string detail_;
  explicit SocketError(SocketErrorCode code, std::string detail,
                       int err) noexcept
      : code_(code), errno_(err), detail_(std::move(detail)) {}

 public:
  explicit SocketError(SocketErrorCode code, std::string detail = "") noexcept
      : SocketError(code, std::move(detail), 0) {}

  static SocketError from_errno(SocketErrorCode code,
                                std::string detail = "") noexcept;

  [[nodiscard]] SocketErrorCode code() const noexcept { return code_; }

  [[nodiscard]] int system_errno() const noexcept { return errno_; }

  [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

  [[nodiscard]] const char* code_name() const noexcept;

  [[nodiscard]] std::string message() const noexcept;
};

template <typename T = void>
using SocketResult = Result<T, SocketError>;

}  // namespace tx::network

#endif
