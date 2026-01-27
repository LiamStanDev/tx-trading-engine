#include "tx/net/taifex/parser.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "tx/net/taifex/error.hpp"
#include "tx/net/taifex/wire_format.hpp"

namespace tx::net::taifex::text {

// ============================================================================
// 輔助函數
// ============================================================================

/**
 * @brief Mock Packet Header
 */
std::vector<std::byte> make_mock_packet_header() {
  std::vector<std::byte> buffer(sizeof(PacketHeader));
  auto* hdr = reinterpret_cast<PacketHeader*>(buffer.data());

  hdr->esc_code = 0x1B;
  hdr->packet_version = 0x01;

  hdr->packet_length = htons(sizeof(PacketHeader));  // = 16 + 90 = 106 bytes

  hdr->msg_count = htons(2);
  hdr->pkt_seq_num = htonl(12345);
  hdr->channel_id = htons(1);
  hdr->send_time = htonl(13305500);  // 13:30:55.00

  return buffer;
}

// ============================================================================
// Packet Header 測試
// ============================================================================
TEST(TaifexParser, ParsePacketHeader_Valid) {
  auto buffer = make_mock_packet_header();

  auto result = parse_packet_header(buffer);
  ASSERT_TRUE(result) << result.error().message();
  auto& parsed = *result;
  EXPECT_EQ(parsed.esc_code, 0x1B);
  EXPECT_EQ(parsed.packet_version, 0x01);
  EXPECT_EQ(parsed.packet_length, sizeof(PacketHeader));
  EXPECT_EQ(parsed.msg_count, 2);
  EXPECT_EQ(parsed.pkt_seq_num, 12345);
  EXPECT_EQ(parsed.channel_id, 1);
  EXPECT_EQ(parsed.send_time, 13305500);
}

TEST(TaifexParser, ParsePacketHeader_InvalidEscCode) {
  auto buffer = make_mock_packet_header();
  buffer[0] = std::byte{0xFF};  // 錯誤的 EscCode

  auto result = parse_packet_header(buffer);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(parse_errc::invalid_esc_code));
}
TEST(TaifexParser, ParsePacketHeader_BufferTooSmall) {
  std::vector<std::byte> buffer(10);  // 小於 16 bytes

  auto result = parse_packet_header(buffer);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(parse_errc::buffer_too_small));
}

}  // namespace tx::net::taifex::text
