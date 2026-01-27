#ifndef TX_TRADING_ENGINE_IO_UDP_SOCKET_HPP
#define TX_TRADING_ENGINE_IO_UDP_SOCKET_HPP

#include "tx/error.hpp"
#include "tx/io/socket.hpp"
#include "tx/io/socket_address.hpp"

namespace tx::io {

/// @brief UDP Socket 封裝 (支援 Multicast)
class UdpSocket {
 private:
  Socket socket_;
  explicit UdpSocket(Socket socket) noexcept;

 public:
  // ==================================
  // Factory Methods
  // ==================================
  // @brief 建立 UDP Socket
  // @details 適用於 Client/Server 通用場景
  static Result<UdpSocket> create() noexcept;

  // @brief 建立並綁定 UDP Socket
  // @param local_addr 本機地址
  // @details 適用於 Server 端或者 Multicast 接收端
  static Result<UdpSocket> bind(const SocketAddress& local_addr) noexcept;

  // ==================================
  // RAII 管理
  // ==================================
  ~UdpSocket() noexcept = default;
  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;
  UdpSocket(UdpSocket&&) = default;
  UdpSocket& operator=(UdpSocket&&) = default;

  // ==================================
  // Multicast 操作
  // ==================================
  /// @brief 加入 Multicast group (接收端)
  /// @param multicast_addr Multicast 地址 (224.0.0.0 ~ 239.255.255.255)
  /// @param interface_addr 網卡地址 (0.0.0.0 = 自動選擇)
  /// @return 成功或錯誤
  /// @details 內部使用 IP_ADD_MEMBERSHIP
  Result<> join_multicast_group(const SocketAddress& multicast_addr,
                                const SocketAddress& interface_addr =
                                    SocketAddress::any_ipv4(0)) noexcept {
    return socket_.join_multicast_group(multicast_addr, interface_addr);
  }

  /// @brief 離開 Multicast group
  /// @param multicast_addr Multicast 地址 (224.0.0.0 ~ 239.255.255.255)
  /// @param interface_addr 網卡地址
  /// @return 成功或錯誤
  /// @details 內部使用 IP_DROP_MEMBERSHIP
  Result<> leave_multicast_group(const SocketAddress& multicast_addr,
                                 const SocketAddress& interface_addr =
                                     SocketAddress::any_ipv4(0)) noexcept {
    return socket_.leave_multicast_group(multicast_addr, interface_addr);
  }

  /// @brief 設定 Multicast TTL (發送端)
  /// @param ttl Time-to-Live
  /// @return 成功或錯誤
  ///  - 1: 同一子網
  ///  - 32: 同一地區
  ///  - 255: 全球
  Result<> set_multicast_ttl(int ttl = 1) noexcept {
    return socket_.set_multicast_ttl(ttl);
  }

  /// @brief 設定是否接收自己發送的 Multicast 封包
  /// @details 內部使用 IP_MULTICAST_LOOP（預設 true）
  Result<> set_multicast_loopback(bool enable) noexcept {
    return socket_.set_multicast_loopback(enable);
  }

  // ===========================
  // I/O 操作
  // ===========================
  /// @brief 發送數據到指定地址
  /// @param data 要發送的位元組序列
  /// @param dest 目標地址（可以是 Unicast 或 Multicast）
  /// @return 實際發送的位元組數
  /// @details
  ///   - UDP 保證原子性（整個封包發送或失敗）
  ///   - 單個封包不超過 MTU（通常 1500 bytes）
  ///   - 超過 MTU 會被 IP 層分片（可能丟失）
  Result<size_t> sendto(std::span<const std::byte> data,
                        const SocketAddress& dest) noexcept {
    return socket_.sendto(data, dest);
  }

  /// @brief 接收數據（並取得發送端地址）
  /// @param buffer 接收緩衝區
  /// @param src 發送端地址（可選，nullptr = 不需要）
  /// @return 實際接收的位元組數
  /// @details
  ///   - 單次接收一個完整封包
  ///   - buffer 太小會截斷（MSG_TRUNC）
  ///   - 建議 buffer >= 65536（UDP 最大封包）
  Result<size_t> recvfrom(std::span<std::byte> buffer,
                          SocketAddress* src = nullptr) noexcept {
    return socket_.recvfrom(buffer, src);
  }

  // ===========================
  // Socket 選項
  // ===========================
  /// @brief 設定接收緩衝區大小（防止丟包）
  /// @param size 建議 4MB ~ 8MB for market data
  /// @return 成功或錯誤
  /// @details
  ///   - 系統限制：/proc/sys/net/core/rmem_max
  ///   - 如果超過限制，需要 sudo sysctl -w net.core.rmem_max=8388608
  Result<> set_recv_buffer_size(int size) noexcept {
    return socket_.set_recv_buffer_size(size);
  }

  /// @brief 設定發送緩衝區大小
  /// @return 成功或錯誤
  Result<> set_send_buffer_size(int size) noexcept {
    return socket_.set_send_buffer_size(size);
  }

  /// @brief 設定非阻塞模式
  /// @return 成功或錯誤
  Result<> set_nonblocking(bool enable) noexcept {
    return socket_.set_nonblocking(enable);
  }

  // ===========================
  // 查詢函數
  // ===========================
  /// @brief 檢查 socket 是否有效
  [[nodiscard]] bool is_valid() const noexcept { return socket_.is_valid(); }

  /// @brief 取得本地地址
  [[nodiscard]] Result<SocketAddress> local_address() const noexcept {
    return socket_.local_address();
  }

  /// @brief 取得底層 Socket
  [[nodiscard]] const Socket& raw_socket() const noexcept { return socket_; }
};

}  // namespace tx::io

#endif
