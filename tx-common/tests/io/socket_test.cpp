#include "tx/io/socket.hpp"

#include <gtest/gtest.h>

#include "gtest/gtest.h"
#include "tx/io/socket_address.hpp"
namespace tx::io::test {

class SocketTest : public ::testing::Test {};

// ----------------------------------------------------------------------------
// Factory & RAII
// ----------------------------------------------------------------------------

TEST_F(SocketTest, CreateTCP_Success) {
  auto sock = Socket::create_tcp();

  ASSERT_TRUE(socket);
  EXPECT_TRUE(sock->is_valid());
  EXPECT_GE(sock->fd(), 0);
}

TEST_F(SocketTest, Create_UDP_Success) {
  auto sock = Socket::create_udp();

  ASSERT_TRUE(socket);
  EXPECT_TRUE(sock->is_valid());
  EXPECT_GE(sock->fd(), 0);
}

TEST_F(SocketTest, MoveSemantics_TransferOwnership) {
  auto sock1 = Socket::create_tcp();
  ASSERT_TRUE(socket);

  int fd = sock1->fd();

  // Move construction
  Socket sock2 = std::move(*sock1);
  EXPECT_TRUE(sock2.is_valid());
  EXPECT_EQ(sock2.fd(), fd);
  EXPECT_FALSE(sock1->is_valid());

  // Move assignment
  auto sock3 = Socket::create_tcp();
  ASSERT_TRUE(sock3);
  *sock3 = std::move(sock2);
  EXPECT_TRUE(sock3->is_valid());
  EXPECT_EQ(sock2.fd(), -1);
}

// ----------------------------------------------------------------------------
// TCP Operations
// ----------------------------------------------------------------------------

TEST_F(SocketTest, Bind_Success) {
  auto sock = Socket::create_tcp();
  ASSERT_TRUE(sock);

  auto addr = SocketAddress::any(15007);
  auto result = sock->bind(addr);

  ASSERT_TRUE(result) << "bind failed: " << result.error().message();
}

TEST_F(SocketTest, Listen_Success) {
  auto sock = Socket::create_tcp();
  ASSERT_TRUE(sock);

  auto addr = SocketAddress::any(15008);
  ASSERT_TRUE(sock->bind(addr));

  auto result = sock->listen(128);
  ASSERT_TRUE(result) << "listen failed: " << result.error().message();
}

TEST_F(SocketTest, TCP_ConnectAccept_Success) {
  uint16_t port = 15002;

  // Server socket
  auto server = Socket::create_tcp();
  ASSERT_TRUE(server);
  ASSERT_TRUE(server->bind(SocketAddress::any(port)));
  ASSERT_TRUE(server->listen(1));

  // Client 在另一個線程連接
  std::thread client_thread([port]() {
    auto client = Socket::create_tcp();
    ASSERT_TRUE(client);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto result =
        client->connect(SocketAddress::from("127.0.0.1", port).value());
    EXPECT_TRUE(result) << "connect failed: " << result.error().message();
  });

  // Server accept
  SocketAddress client_addr;
  auto accepted = server->accept(&client_addr);

  client_thread.join();

  ASSERT_TRUE(accepted) << "accept failed: " << accepted.error().message();
  EXPECT_TRUE(accepted->is_valid());
}

TEST_F(SocketTest, TCP_SendRecv_Success) {
  uint16_t port = 15004;

  // Server
  auto server = Socket::create_tcp();
  ASSERT_TRUE(server);
  ASSERT_TRUE(server->bind(SocketAddress::any(port)));
  ASSERT_TRUE(server->listen(1));

  std::vector<std::byte> received_data;

  std::thread server_thread([&server, &received_data]() {
    auto client = server->accept();
    ASSERT_TRUE(client);

    std::vector<std::byte> buf(100);
    auto n = client->recv(buf);
    ASSERT_TRUE(n);

    received_data.assign(buf.begin(), buf.begin() + *n);
  });

  // Client
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto client = Socket::create_tcp();
  ASSERT_TRUE(client);
  ASSERT_TRUE(client->connect(SocketAddress::from("127.0.0.1", port).value()));

  std::string message = "Hello, Socket!";
  auto bytes = std::as_bytes(std::span(message));
  auto sent = client->send(bytes);
  ASSERT_TRUE(sent);
  EXPECT_EQ(*sent, message.size());

  server_thread.join();

  // 驗證收到的資料
  std::string received(reinterpret_cast<char*>(received_data.data()),
                       received_data.size());
  EXPECT_EQ(received, message);
}

// ----------------------------------------------------------------------------
// UDP Operations
// ----------------------------------------------------------------------------

TEST_F(SocketTest, UDP_SendtoRecvfrom_Success) {
  uint16_t port = 15005;

  // Receiver
  auto receiver = Socket::create_udp();
  ASSERT_TRUE(receiver);
  ASSERT_TRUE(receiver->bind(SocketAddress::any(port)));

  // Sender
  auto sender = Socket::create_udp();
  ASSERT_TRUE(sender);

  // Send
  std::string message = "UDP Test";
  auto bytes = std::as_bytes(std::span(message));
  auto dest = SocketAddress::from("127.0.0.1", port).value();
  auto sent = sender->sendto(bytes, dest);

  ASSERT_TRUE(sent);
  EXPECT_EQ(*sent, message.size());

  // Receive
  std::vector<std::byte> buf(100);
  SocketAddress src;
  auto received = receiver->recvfrom(buf, &src);

  ASSERT_TRUE(received);
  EXPECT_EQ(*received, message.size());
  EXPECT_EQ(src.ip(), "127.0.0.1");

  std::string received_msg(reinterpret_cast<char*>(buf.data()), *received);
  EXPECT_EQ(received_msg, message);
}

TEST_F(SocketTest, UDP_RecvfromWithoutSource) {
  uint16_t port = 15006;

  auto receiver = Socket::create_udp();
  ASSERT_TRUE(receiver);
  ASSERT_TRUE(receiver->bind(SocketAddress::any(port)));

  auto sender = Socket::create_udp();
  ASSERT_TRUE(sender);

  std::string message = "Test";
  auto bytes = std::as_bytes(std::span(message));
  ASSERT_TRUE(
      sender->sendto(bytes, SocketAddress::from("127.0.0.1", port).value()));

  // 不關心來源地址 (nullptr)
  std::vector<std::byte> buf(100);
  auto received = receiver->recvfrom(buf, nullptr);

  ASSERT_TRUE(received);
  EXPECT_EQ(*received, message.size());
}

// ----------------------------------------------------------------------------
// Options
// ----------------------------------------------------------------------------

TEST_F(SocketTest, SetNonblocking_Success) {
  auto sock = Socket::create_tcp();
  ASSERT_TRUE(sock);

  auto result = sock->set_nonblocking(true);
  ASSERT_TRUE(result) << "set_nonblocking failed: " << result.error().message();

  // 恢復 blocking mode
  ASSERT_TRUE(sock->set_nonblocking(false));
}

TEST_F(SocketTest, SetTCPNodelay_Success) {
  auto sock = Socket::create_tcp();
  ASSERT_TRUE(sock);

  auto result = sock->set_tcp_nodelay(true);
  ASSERT_TRUE(result) << "set_tcp_nodelay failed: " << result.error().message();
}

// ----------------------------------------------------------------------------
// Multicast
// ----------------------------------------------------------------------------

TEST_F(SocketTest, JoinMulticastGroup_ValidAddress) {
  auto sock = Socket::create_udp();
  ASSERT_TRUE(sock);

  // 239.1.1.1 是有效的 multicast 地址
  auto mcast_addr = SocketAddress::from("239.1.1.1", 5000);
  auto iface_addr = SocketAddress::from("0.0.0.0", 0);

  auto result = sock->join_multicast_group(*mcast_addr, *iface_addr);
  ASSERT_TRUE(result) << "join_multicast_group failed: "
                      << result.error().message();
}

TEST_F(SocketTest, JoinMulticastGroup_InvalidAddress_ReturnsError) {
  auto sock = Socket::create_udp();
  ASSERT_TRUE(sock);

  // 192.168.1.1 不是 multicast 地址
  auto invalid_addr = SocketAddress::from("192.168.1.1", 5000);
  auto iface_addr = SocketAddress::from("0.0.0.0", 0);

  auto result = sock->join_multicast_group(*invalid_addr, *iface_addr);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------

TEST_F(SocketTest, LocalAddress_AfterBind) {
  auto sock = Socket::create_tcp();
  ASSERT_TRUE(sock);

  uint16_t port = 15001;
  auto bind_addr = SocketAddress::any(port);
  ASSERT_TRUE(sock->bind(bind_addr));

  auto local = sock->local_address();
  ASSERT_TRUE(local);
  EXPECT_EQ(local->port(), port);
}

}  // namespace tx::io::test
