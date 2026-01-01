#ifndef TX_TRADING_ENGINE_NETWORK_TCP_SOCKET_HPP
#define TX_TRADING_ENGINE_NETWORK_TCP_SOCKET_HPP

#include "tx/network/socket.hpp"
#include "tx/network/socket_address.hpp"

namespace tx::network {

class TcpSocket {
 private:
  Socket socket_;

  explicit TcpSocket(Socket socket) noexcept : socket_(std::move(socket)) {}

 public:
  // ==========================
  // Factory Methods
  // ==========================
  /// @brief 建立 TCP Client 連線
  /// @param remote_addr 遠端地址
  /// @param nodelay 是否禁用 Nagle 算法（預設 true，低延遲優先）
  /// @return TcpSocket 或錯誤
  static Result<TcpSocket> connect(const SocketAddress& remote_addr,
                                   bool nodelay = true) noexcept;
  /// @brief 建立 TCP Server 監聽
  /// @param local_addr 本地綁定地址
  /// @param backlog 連線佇列大小（預設 128）
  /// @return TcpSocket（處於監聽狀態）或錯誤
  static Result<TcpSocket> serve(const SocketAddress& local_addr,
                                 int backlog = 128) noexcept;

  // ==========================
  // RAII 管理
  // ==========================
  ~TcpSocket() noexcept = default;

  /// @brief 禁止複製
  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;

  /// @brief 允許移動
  TcpSocket(TcpSocket&&) = default;
  TcpSocket& operator=(TcpSocket&&) = default;

  // ==========================
  // Server 端操作
  // ==========================

  /// @brief 接受新連線（Server 端）
  /// @param client_addr 客戶端地址（可選）
  /// @return 新的 TcpSocket（代表客戶端連線）
  Result<TcpSocket> accept(SocketAddress* client_addr = nullptr) noexcept;

  // ==========================
  // I/O 操作
  // ==========================
  /// @brief 發送數據
  Result<size_t> send(std::span<const std::byte> data) noexcept {
    return socket_.send(data);
  }

  /// @brief 接收數據
  Result<size_t> recv(std::span<std::byte> buffer) noexcept {
    return socket_.recv(buffer);
  }

  // ==========================
  // Socket 選項
  // ==========================
  /// @brief 設定 TCP_NODELAY（禁用 Nagle 算法）
  Result<> set_nodelay(bool enable) noexcept {
    return socket_.set_tcp_nodelay(enable);
  }

  /// @brief 設定 SO_KEEPALIVE（啟用 TCP Keepalive）
  Result<> set_keepalive(bool enable) noexcept {
    return socket_.set_tcp_keepalive(enable);
  }

  /// @brief 設定非阻塞模式
  Result<> set_nonblocking(bool enable) noexcept {
    return socket_.set_nonblocking(enable);
  }

  // ==========================
  // 查詢函數
  // ==========================
  /// @brief 檢查 socket 是否有效
  [[nodiscard]] bool is_valid() const noexcept { return socket_.is_valid(); }

  /// @brief 取得本地地址
  [[nodiscard]] Result<SocketAddress> local_address() const noexcept {
    return socket_.local_address();
  }

  /// @brief 取得遠端地址
  [[nodiscard]] Result<SocketAddress> remote_address() const noexcept {
    return socket_.remote_address();
  }

  /// @brief 取得底層 Socket（給進階使用者）
  [[nodiscard]] const Socket& raw_socket() const noexcept { return socket_; }
};

}  // namespace tx::network

#endif
