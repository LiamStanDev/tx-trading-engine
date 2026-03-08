#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstring>

#include "onyx/net/taifex/codec.hpp"
#include "onyx/net/taifex/message.hpp"
#include "onyx/net/taifex/parser.hpp"
#include "onyx/net/taifex/wire_format.hpp"

namespace {

using namespace onyx::net::taifex;

std::vector<std::byte> build_i024_buffer(const char* prod_id, uint64_t prod_msg_seq,
                                         uint64_t match_time, uint64_t first_match_price,
                                         uint64_t first_match_qty, char calculated_flag,
                                         uint8_t match_count, uint64_t total_qty,
                                         uint64_t buy_count, uint64_t sell_count) {
  size_t total_size =
      sizeof(I024FixedPart) + match_count * sizeof(I024MatchEntry) + sizeof(I024Footer);

  std::vector<std::byte> buf(total_size, std::byte{0});

  // --- fixed part ---
  auto* fixed = reinterpret_cast<I024FixedPart*>(buf.data());
  // header
  fixed->header.esc_code = 27;  // ASCII ESC
  fixed->header.transmission_code = '2';
  fixed->header.message_kind = 'D';
  encode_bcd(fixed->header.info_time, 6, 90000000000ULL);  // 09:00:00.000000
  encode_bcd(fixed->header.channel_id, 2, 1);
  encode_bcd(fixed->header.channel_seq, 5, 1);
  fixed->header.version_no = 0x01;
  encode_bcd(fixed->header.body_length, 2, static_cast<uint64_t>(total_size - 19));

  // body
  std::memcpy(fixed->prod_id, prod_id, std::min(std::strlen(prod_id), size_t{20}));
  for (size_t i = strlen(prod_id); i < 20; ++i) {
    fixed->prod_id[i] = ' ';
  }

  encode_bcd(fixed->prod_msg_seq, 5, prod_msg_seq);
  fixed->calculated_flag = calculated_flag;
  encode_bcd(fixed->match_time, 6, match_time);
  fixed->sign = '+';
  encode_bcd(fixed->first_match_price, 5, first_match_price);
  encode_bcd(fixed->first_match_qty, 4, first_match_qty);

  // match_display_item: Bit 7 = 1（第一個封包），Bit 6-0 = match_count
  fixed->match_display_item = 0x80 | (match_count & 0x7F);

  // --- footer ---
  size_t footer_offset = sizeof(I024FixedPart) + match_count * sizeof(I024MatchEntry);
  auto* footer = reinterpret_cast<I024Footer*>(buf.data() + footer_offset);

  encode_bcd(footer->match_total_qty, 4, total_qty);
  encode_bcd(footer->match_buy_cnt, 4, buy_count);
  encode_bcd(footer->match_sell_cnt, 4, sell_count);
  footer->check_sum = 'X';  // 簡化，不計算真實 checksum
  footer->terminal_code[0] = 0x0D;
  footer->terminal_code[1] = 0x0A;

  return buf;
}

}  // namespace

// ============================================================================
// 成功案例測試
// ============================================================================

// ----------------------------------------------------------------------------
// 單筆成交
// ----------------------------------------------------------------------------

TEST(Parser, ParseI024_SingleTrade) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  auto buffer = build_i024_buffer(prod_id,         // prod_id
                                  12345,           // prod_msg_seq
                                  90000123456ULL,  // match_time: 09:00:00.123456
                                  1850050,  // first_match_price: 18500.50（decimal_places = 2）
                                  5,        // first_match_qty
                                  '0',      // calculated_flag: 成交揭示
                                  0,        // match_count: 只有第一筆成交
                                  100,      // total_qty
                                  60,       // buy_count
                                  40        // sell_count
  );

  auto result = parse_i024(buffer, spec_table);

  ASSERT_TRUE(result);
  const Trade& trade = *result;
  EXPECT_EQ(trade.prod_id[0], 'T');
  EXPECT_EQ(trade.prod_id[1], 'X');
  EXPECT_EQ(trade.prod_id[2], 'F');
  EXPECT_EQ(trade.prod_id[3], '\0');  // trim 後填充 '\0'
  EXPECT_EQ(trade.prod_msg_seq, 12345);
  std::chrono::nanoseconds expected_time = std::chrono::hours(9) + std::chrono::seconds(0) +
                                           std::chrono::milliseconds(123) +
                                           std::chrono::microseconds(456);
  EXPECT_EQ(trade.match_time, expected_time);
  EXPECT_DOUBLE_EQ(trade.price.to_double(), 18500.50);
  EXPECT_EQ(trade.qty.value(), 5);
  EXPECT_FALSE(trade.is_calculated);
  EXPECT_EQ(trade.total_qty.value(), 100);
  EXPECT_EQ(trade.buy_count, 60);
  EXPECT_EQ(trade.sell_count, 40);
}

// ----------------------------------------------------------------------------
// 多筆成交
// ----------------------------------------------------------------------------
TEST(Parser, ParseI024_MultipleTrades) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  // 構造一個有 2 筆額外成交的 buffer
  auto buffer = build_i024_buffer(prod_id, 1, 90000000000ULL, 18500, 5, '0',
                                  2,  // match_count = 2
                                  100, 60, 40);

  auto result = parse_i024(buffer, spec_table);

  ASSERT_TRUE(result);

  // 驗證 Footer 欄位
  EXPECT_EQ(result->total_qty.value(), 100);
  EXPECT_EQ(result->buy_count, 60);
  EXPECT_EQ(result->sell_count, 40);
}

// ----------------------------------------------------------------------------
// 試搓標記
// ----------------------------------------------------------------------------

TEST(Parser, ParseI024_CalculatedFlag) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  auto buffer = build_i024_buffer(prod_id, 1, 90000000000ULL, 18500, 5,
                                  '1',  // calculated_flag = '1'
                                  0, 100, 60, 40);

  auto result = parse_i024(buffer, spec_table);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_calculated);
}

// ============================================================================
// 邊界條件測試
// ============================================================================

// ----------------------------------------------------------------------------
// Buffer 長度不足
// ----------------------------------------------------------------------------

TEST(Parser, ParseI024_BufferTooSmall) {
  ProductSpecTable spec_table;

  // 長度小於 sizeof(I024)
  std::vector<std::byte> buf(10, std::byte{0});

  auto result = parse_i024(buf, spec_table);
  EXPECT_FALSE(result);
}

TEST(Parser, ParseI024_MismatchedLength) {
  ProductSpecTable spec_table;

  // 構造一個有 3 筆額外成交的 buffer
  auto buffer = build_i024_buffer("TXF", 1, 90000000000ULL, 18500, 5, '0',
                                  3,  // match_count = 3（需要 3 * 8 = 24 bytes）
                                  100, 60, 40);

  // 刻意截斷 buffer，只保留最小長度
  buffer.resize(77);  // 最小長度，不夠放 3 筆 MatchEntry

  auto result = parse_i024(buffer, spec_table);
  EXPECT_FALSE(result);
}

// ----------------------------------------------------------------------------
// 商品不存在於 ProductSpecTable
// ----------------------------------------------------------------------------

TEST(Parser, ParseI024_UnknownProduct) {
  ProductSpecTable spec_table;

  auto buffer = build_i024_buffer("UNKNOWN",       // prod_id
                                  1,               // prod_msg_seq
                                  90000000000ULL,  // match_time: 09:00:00.000000
                                  18500,           // first_match_price
                                  5,               // first_match_qty
                                  '0',             // calculated_flag
                                  0,               // match_count
                                  100,             // total_qty
                                  60,              // buy_count
                                  40               // sell_count
  );

  auto result = parse_i024(buffer, spec_table);
  EXPECT_FALSE(result);
}
