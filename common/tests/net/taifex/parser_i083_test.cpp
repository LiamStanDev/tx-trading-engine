#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <vector>

#include "onyx/net/taifex/codec.hpp"
#include "onyx/net/taifex/constant.hpp"
#include "onyx/net/taifex/message.hpp"
#include "onyx/net/taifex/parser.hpp"
#include "onyx/net/taifex/wire_format.hpp"

namespace {

using namespace onyx::net::taifex;

struct BookEntry {
  EntryType md_entry_type;  // '0': 買, '1': 賣, 'E': 衍生買, 'F': 衍生賣
  Sign sign;
  uint64_t price;
  uint64_t size;
  uint8_t level;  // 1-5
};

std::vector<std::byte> build_i083_buffer(const char* prod_id, uint64_t prod_msg_seq,
                                         CalculatedFlag calculated_flag,
                                         const std::vector<BookEntry>& entries) {
  uint8_t entry_count = static_cast<uint8_t>(entries.size());
  size_t total_size = sizeof(I083FixedPart) + entry_count * sizeof(I083Entry) + sizeof(I083Footer);

  std::vector<std::byte> buf(total_size, std::byte{0});

  // --- fixed part ---
  auto* fixed = reinterpret_cast<I083FixedPart*>(buf.data());

  // header
  fixed->header.esc_code = 27;  // ASCII ESC
  fixed->header.transmission_code = '2';
  fixed->header.message_kind = 'S';
  encode_bcd(fixed->header.info_time, 6, 84500000000ULL);  // 08:45:00.000000
  encode_bcd(fixed->header.channel_id, 2, 1);
  encode_bcd(fixed->header.channel_seq, 5, 1);
  fixed->header.version_no = 0x01;
  encode_bcd(fixed->header.body_length, 2, static_cast<uint64_t>(total_size - 19));

  // body
  std::memcpy(fixed->prod_id, prod_id, std::min(std::strlen(prod_id), size_t{20}));
  for (size_t i = std::strlen(prod_id); i < 20; ++i) {
    fixed->prod_id[i] = ' ';
  }

  encode_bcd(fixed->prod_msg_seq, 5, prod_msg_seq);
  fixed->calculated_flag = calculated_flag;
  encode_bcd(&fixed->no_md_entries, 1, entry_count);

  // --- entries ---
  if (entry_count > 0) {
    auto* entry_ptr = reinterpret_cast<I083Entry*>(buf.data() + sizeof(I083FixedPart));
    for (size_t i = 0; i < entries.size(); ++i) {
      entry_ptr[i].md_entry_type = entries[i].md_entry_type;
      entry_ptr[i].sign = entries[i].sign;
      encode_bcd(entry_ptr[i].md_entry_px, 5, entries[i].price);
      encode_bcd(entry_ptr[i].md_entry_size, 4, entries[i].size);
      encode_bcd(&entry_ptr[i].md_price_level, 1, entries[i].level);
    }
  }

  // --- footer ---
  size_t footer_offset = sizeof(I083FixedPart) + entry_count * sizeof(I083Entry);
  auto* footer = reinterpret_cast<I083Footer*>(buf.data() + footer_offset);

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
// 基本解析：單一買賣檔
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_SingleBidAsk) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  // 買一檔 18500.50 x 10, 賣一檔 18500.60 x 5
  std::vector<BookEntry> entries = {
      {EntryType::Bid, Sign::Positive, 1850050, 10, 1},  // 買一檔
      {EntryType::Ask, Sign::Positive, 1850060, 5, 1},   // 賣一檔
  };

  auto buffer = build_i083_buffer(prod_id, 12345, CalculatedFlag::Real, entries);

  auto result = parse_i083(buffer, spec_table);

  ASSERT_TRUE(result);
  const BookSnapshot& snapshot = *result;

  EXPECT_EQ(snapshot.prod_id[0], 'T');
  EXPECT_EQ(snapshot.prod_id[1], 'X');
  EXPECT_EQ(snapshot.prod_id[2], 'F');
  EXPECT_EQ(snapshot.prod_id[3], ' ');
  EXPECT_EQ(snapshot.prod_msg_seq, 12345);
  EXPECT_FALSE(snapshot.is_calculated);

  EXPECT_DOUBLE_EQ(snapshot.bids[0].price.value(), 18500.50);
  EXPECT_EQ(snapshot.bids[0].size.value(), 10);

  EXPECT_DOUBLE_EQ(snapshot.asks[0].price.value(), 18500.60);
  EXPECT_EQ(snapshot.asks[0].size.value(), 5);
}

// ----------------------------------------------------------------------------
// 多檔位：5 檔買賣
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_FiveLevels) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  // 買 1-5 檔 + 賣 1-5 檔
  std::vector<BookEntry> entries = {
      {EntryType::Bid, Sign::Positive, 1850050, 10, 1},  // 買一
      {EntryType::Bid, Sign::Positive, 1850040, 8, 2},   // 買二
      {EntryType::Bid, Sign::Positive, 1850030, 6, 3},   // 買三
      {EntryType::Bid, Sign::Positive, 1850020, 4, 4},   // 買四
      {EntryType::Bid, Sign::Positive, 1850010, 2, 5},   // 買五
      {EntryType::Ask, Sign::Positive, 1850060, 5, 1},   // 賣一
      {EntryType::Ask, Sign::Positive, 1850070, 7, 2},   // 賣二
      {EntryType::Ask, Sign::Positive, 1850080, 9, 3},   // 賣三
      {EntryType::Ask, Sign::Positive, 1850090, 11, 4},  // 賣四
      {EntryType::Ask, Sign::Positive, 1850100, 13, 5},  // 賣五
  };

  auto buffer = build_i083_buffer(prod_id, 1, CalculatedFlag::Real, entries);

  auto result = parse_i083(buffer, spec_table);

  ASSERT_TRUE(result);

  EXPECT_DOUBLE_EQ(result->bids[0].price.value(), 18500.50);
  EXPECT_EQ(result->bids[0].size.value(), 10);
  EXPECT_DOUBLE_EQ(result->bids[4].price.value(), 18500.10);
  EXPECT_EQ(result->bids[4].size.value(), 2);

  EXPECT_DOUBLE_EQ(result->asks[0].price.value(), 18500.60);
  EXPECT_EQ(result->asks[0].size.value(), 5);
  EXPECT_DOUBLE_EQ(result->asks[4].price.value(), 18501.00);
  EXPECT_EQ(result->asks[4].size.value(), 13);
}

// ----------------------------------------------------------------------------
// 衍生檔位：E (衍生買) 和 F (衍生賣)
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_DerivedLevels) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  std::vector<BookEntry> entries = {
      {EntryType::Bid, Sign::Positive, 1850050, 10, 1},        // 買一
      {EntryType::Ask, Sign::Positive, 1850060, 5, 1},         // 賣一
      {EntryType::DerivedBid, Sign::Positive, 1850045, 3, 1},  // 衍生買一
      {EntryType::DerivedAsk, Sign::Positive, 1850065, 2, 1},  // 衍生賣一
  };

  auto buffer = build_i083_buffer(prod_id, 1, CalculatedFlag::Real, entries);

  auto result = parse_i083(buffer, spec_table);

  ASSERT_TRUE(result);

  EXPECT_DOUBLE_EQ(result->derived_bid.price.value(), 18500.45);
  EXPECT_EQ(result->derived_bid.size.value(), 3);

  EXPECT_DOUBLE_EQ(result->derived_ask.price.value(), 18500.65);
  EXPECT_EQ(result->derived_ask.size.value(), 2);
}

// ----------------------------------------------------------------------------
// 試撮標記
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_CalculatedFlag) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  std::vector<BookEntry> entries = {
      {EntryType::Bid, Sign::Positive, 1850050, 10, 1},
      {EntryType::Ask, Sign::Positive, 1850060, 5, 1},
  };

  auto buffer = build_i083_buffer(prod_id, 1,
                                  CalculatedFlag::Simulated,  // calculated_flag = '1'
                                  entries);

  auto result = parse_i083(buffer, spec_table);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_calculated);
}

// ----------------------------------------------------------------------------
// 負價格（sign = '-'）
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_NegativePrice) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  std::vector<BookEntry> entries = {
      BookEntry{EntryType::Bid, Sign::Negative, 1850050, 10, 1},  // 買一檔，負價格
      BookEntry{EntryType::Ask, Sign::Positive, 1850060, 5, 1},   // 賣一檔
  };

  auto buffer = build_i083_buffer(prod_id, 1, CalculatedFlag::Real, entries);

  auto result = parse_i083(buffer, spec_table);

  ASSERT_TRUE(result);

  EXPECT_DOUBLE_EQ(result->bids[0].price.value(), -18500.50);
  EXPECT_EQ(result->bids[0].size.value(), 10);
}

// ============================================================================
// 邊界條件測試
// ============================================================================

// ----------------------------------------------------------------------------
// Buffer 長度不足
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_BufferTooSmall) {
  ProductSpecTable spec_table;

  // 長度小於 sizeof(I083FixedPart) + sizeof(I083Footer)
  std::vector<std::byte> buf(10, std::byte{0});

  auto result = parse_i083(buf, spec_table);
  EXPECT_FALSE(result);
}

TEST(Parser, ParseI083_MismatchedLength) {
  ProductSpecTable spec_table;

  std::vector<BookEntry> entries = {
      BookEntry{EntryType::Bid, Sign::Positive, 1850050, 10, 1},
      BookEntry{EntryType::Bid, Sign::Positive, 1850040, 8, 2},
      BookEntry{EntryType::Bid, Sign::Positive, 1850030, 6, 3},
  };

  // 構造一個有 3 筆 entry 的 buffer
  auto buffer = build_i083_buffer("TXF", 1, CalculatedFlag::Real, entries);

  // 刻意截斷 buffer，不夠放 3 筆 Entry + Footer
  buffer.resize(sizeof(I083FixedPart) + 2 * sizeof(I083Entry) + sizeof(I083Footer) - 1);

  auto result = parse_i083(buffer, spec_table);
  EXPECT_FALSE(result);
}

// ----------------------------------------------------------------------------
// 商品不存在於 ProductSpecTable
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_UnknownProduct) {
  ProductSpecTable spec_table;

  std::vector<BookEntry> entries = {
      BookEntry{EntryType::Bid, Sign::Positive, 1850050, 10, 1},
      BookEntry{EntryType::Ask, Sign::Positive, 1850060, 5, 1},
  };

  auto buffer = build_i083_buffer("UNKNOWN", 1, CalculatedFlag::Real, entries);

  auto result = parse_i083(buffer, spec_table);
  EXPECT_FALSE(result);
}

// ----------------------------------------------------------------------------
// 空委託簿（no_md_entries = 0）
// ----------------------------------------------------------------------------

TEST(Parser, ParseI083_EmptyBook) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  std::vector<BookEntry> entries = {};  // 空的

  auto buffer = build_i083_buffer(prod_id, 1, CalculatedFlag::Real, entries);

  auto result = parse_i083(buffer, spec_table);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->bids[0].size, Quantity::from(0));
  EXPECT_EQ(result->asks[0].size, Quantity::from(0));

  EXPECT_EQ(result->bids[1].size, Quantity::from(0));
  EXPECT_EQ(result->asks[2].size, Quantity::from(0));
}
