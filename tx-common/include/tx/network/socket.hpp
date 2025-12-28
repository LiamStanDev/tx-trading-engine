#ifndef TX_TRADING_ENGINE_NETWORK_SOCKET_HPP
#define TX_TRADING_ENGINE_NETWORK_SOCKET_HPP

#include <cstddef>
#include <span>
#include <tx/core/result.hpp>
#include <tx/network/socket_address.hpp>
#include <tx/network/socket_error.hpp>

namespace tx::network {

/// @brief RAII Socket 封裝
/// @details Move-Only type，自動管理 file descriptor 生命週期
class Socket {
 private:
  int fd_{-1};

  explicit Socket(int fd) noexcept : fd_(fd) {}

 public:
  // ==========================
  // Factory Methods
  // ==========================
  /// @brief 建立 TCP Socket
  [[nodiscard]] static SocketResult<Socket> create_tcp() noexcept;

  /// @brief 建立 UDP Socket
  [[nodiscard]] static SocketResult<Socket> create_udp() noexcept;

  // ==========================
  // RAII 管理
  // ==========================
  /// @brief 解構時關閉 socket
  ~Socket() noexcept;

  /// @brief 禁止複製
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  /// @brief 允許移動
  Socket(Socket&& other) noexcept;
  Socket& operator=(Socket&& other) noexcept;

  // ==========================
  // Socket 操作
  // ==========================
  /// @brief 綁定到本地地址
  SocketResult<> bind(const SocketAddress& addr) noexcept;

  /// @brief 開始監聽
  /// @param backlog 連線佇列大小 (通常為 128)
  SocketResult<> listen(int backlog) noexcept;

  /// @brief 接收新連線 (TCP Server)
  /// @return 新的 Socket (客戶端連線)
  SocketResult<Socket> accept(SocketAddress* client_addr = nullptr) noexcept;

  /// @brief 連線到遠端 (TCP Client)
  SocketResult<> connect(const SocketAddress& addr) noexcept;

  // ==========================
  // I/O 操作
  // ==========================
  /// @brief 發送數據 (TCP)
  /// @param data 要發送的位元組序列
  /// @return 實際發送的位元組數 (可能 < data.size())
  SocketResult<size_t> send(std::span<const std::byte> data) noexcept;

  /// @brief 接收數據 (TCP)
  /// @param buffer 接收緩衝區
  /// @return 實際接收的位元組數 (0 = 對端關閉連線 FIN)
  SocketResult<size_t> recv(std::span<std::byte> buffer) noexcept;

  /// @brief 發送數據到指定地址（UDP）
  /// @param data 要發送的位元組序列
  /// @param dest 目標地址
  /// @return 實際發送的位元組數
  SocketResult<size_t> sendto(std::span<const std::byte> data,
                              const SocketAddress& dest) noexcept;

  /// @brief 從任意地址接收數據（UDP）
  /// @param buffer 接收緩衝區
  /// @param src 發送端地址（可選，nullptr = 不關心來源）
  /// @return 實際接收的位元組數
  SocketResult<size_t> recvfrom(std::span<std::byte> buffer,
                                SocketAddress* src = nullptr) noexcept;

  // ==========================
  // Socket 選項
  // ==========================
  /// @brief 設定非阻塞模式
  SocketResult<> set_nonblocking(bool enable) noexcept;

  /// @brief 設定 SO_REUSEADDR（允許埠號快速重用）
  SocketResult<> set_reuseaddr(bool enable) noexcept;

  /// @brief 設定 TCP_NODELAY（禁用 Nagle 算法，低延遲）
  SocketResult<> set_tcp_nodelay(bool enable) noexcept;

  /// @brief 設定接收緩衝區大小
  SocketResult<> set_recv_buffer_size(int size) noexcept;

  /// @brief 設定發送緩衝區大小
  SocketResult<> set_send_buffer_size(int size) noexcept;

  /// @brief 設定 SO_KEEPALIVE（啟用 TCP Keepalive）
  SocketResult<> set_tcp_keepalive(bool enable) noexcept;

  /// @brief 加入 Multicast group
  SocketResult<> join_multicast_group(const SocketAddress& multicast_addr,
                                      const SocketAddress& interface_addr =
                                          SocketAddress::any_ipv4(0)) noexcept;
  /// @brief 離開 Multicast group
  SocketResult<> leave_multicast_group(const SocketAddress& multicast_addr,
                                       const SocketAddress& interface_addr =
                                           SocketAddress::any_ipv4(0)) noexcept;

  /// @brief 設定 Multicast TTL (發送端)
  SocketResult<> set_multicast_ttl(int ttl) noexcept;

  /// @brief 設定是否接收自己發送的 Multicast 封包
  SocketResult<> set_multicast_loopback(bool enable) noexcept;

  // ==========================
  // 查詢函數
  // ==========================
  /// @brief 檢查 Socket 是否有效
  [[nodiscard]] bool is_valid() const noexcept { return fd_ >= 0; }

  /// @brief 取得原始 file descriptor（給進階使用）
  [[nodiscard]] int fd() const noexcept { return fd_; }

  /// @brief 取得本地位址
  SocketResult<SocketAddress> local_address() const noexcept;

  /// @brief 取得遠端位址
  SocketResult<SocketAddress> remote_address() const noexcept;

  // ==========================
  // 手動管理
  // ==========================
  /// @brief 手動關閉 Socket
  void close() noexcept;
  /// @brief 釋放所有權 (不會自動關閉)
  /// @return 原始 file descriptor
  int release() noexcept;
};
}  // namespace tx::network

#endif
