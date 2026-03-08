#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>

#include "onyx/net/taifex/codec.hpp"
#include "onyx/net/taifex/message.hpp"
#include "onyx/net/taifex/parser.hpp"
#include "onyx/net/taifex/wire_format.hpp"

namespace {

using namespace onyx::net::taifex;

std::vector<std::byte> build_i010_buffer(const char* prod_id_s, uint8_t decimal_loc,
                                         uint8_t flow_group, char prod_kind = 'I') {
  std::vector<std::byte> buf(sizeof(I010), std::byte{0});

  auto* i010 = reinterpret_cast<I010*>(buf.data());

  // --- header ---
  i010->header.esc_code = 27;                             // ASCII ESC
  i010->header.transmission_code = '1';                   // 基本資料群組
  i010->header.message_kind = '1';                        // I010
  encode_bcd(i010->header.info_time, 6, 64500000000ULL);  // 06:45:00.000000
  encode_bcd(i010->header.channel_id, 2, 3);              // Channel 3 基本資料
  encode_bcd(i010->header.channel_seq, 5, 1);
  i010->header.version_no = 0x01;
  encode_bcd(i010->header.body_length, 2, static_cast<uint64_t>(sizeof(I010) - 19));

  // --- body ---
  size_t id_len = std::min(std::strlen(prod_id_s), size_t{10});
  std::memcpy(i010->prod_id_s, prod_id_s, id_len);
  for (size_t i = id_len; i < 10; ++i) {  // 右側補空白
    i010->prod_id_s[i] = ' ';
  }
  i010->prod_kind = prod_kind;
  i010->decimal_locator = decimal_loc;
  i010->flow_group = flow_group;

  // --- footer ---
  i010->check_sum = 'X';  // 簡化，不計算真實 checksum
  i010->terminal_code[0] = 0x0D;
  i010->terminal_code[1] = 0x0A;

  return buf;
}

}  // namespace

// ============================================================================
// 成功案例測試
// ============================================================================

// ----------------------------------------------------------------------------
// 基本解析
// ----------------------------------------------------------------------------

TEST(ParserI010, ParseI010_BasicFutures) {
  auto buf = build_i010_buffer("TXF", 2, 1, 'I');

  auto result = parse_i010(buf);

  ASSERT_TRUE(result);
  const ProductSpec& spec = *result;

  // prod_id 前 3 bytes 應為 'T', 'X', 'F'，其餘補空白
  EXPECT_EQ(spec.prod_id[0], 'T');
  EXPECT_EQ(spec.prod_id[1], 'X');
  EXPECT_EQ(spec.prod_id[2], 'F');
  EXPECT_EQ(spec.prod_id[3], ' ');  // wire 上是空白填充
  EXPECT_EQ(spec.decimal_locator, 2);
  EXPECT_EQ(spec.flow_group, 1);
}

// ----------------------------------------------------------------------------
// 小數位數為 0（整數價格)
// ----------------------------------------------------------------------------

TEST(ParserI010, ParseI010_DecimalLocatorZero) {
  auto buf = build_i010_buffer("MXF", 0, 1, 'I');

  auto result = parse_i010(buf);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->decimal_locator, 0);
}

// ----------------------------------------------------------------------------
// 小數位數為 4（最大值）
// ----------------------------------------------------------------------------

TEST(ParserI010, ParseI010_DecimalLocatorMax) {
  auto buf = build_i010_buffer("TEST", 4, 1, 'I');

  auto result = parse_i010(buf);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->decimal_locator, 4);
}

// ----------------------------------------------------------------------------
// 10 碼滿長度商品代號
// ----------------------------------------------------------------------------

TEST(ParserI010, ParseI010_FullLengthProdId) {
  auto buf = build_i010_buffer("TXO07600A7", 0, 1, 'I');

  auto result = parse_i010(buf);

  ASSERT_TRUE(result);
  // 10 碼全部填滿，沒有空白
  EXPECT_EQ(result->prod_id[0], 'T');
  EXPECT_EQ(result->prod_id[9], '7');
}

// ============================================================================
// 邊界條件測試
// ============================================================================

// ----------------------------------------------------------------------------
// Buffer 長度不足
// ----------------------------------------------------------------------------

TEST(ParserI010, ParseI010_BufferTooSmall) {
  std::vector<std::byte> buf(10, std::byte{0});

  auto result = parse_i010(buf);
  EXPECT_FALSE(result);
}
