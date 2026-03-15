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

struct UpdateEntry {
  UpdateAction action;
  EntryType type;
  Sign sign;
  uint64_t price;
  uint64_t size;
  uint8_t level;  // 1-5 (1-based)
};

std::vector<std::byte> build_i081_buffer(const char* prod_id, uint64_t prod_msg_seq,
                                         const std::vector<UpdateEntry>& entries) {
  uint8_t entry_count = static_cast<uint8_t>(entries.size());
  size_t total_size = sizeof(I081FixedPart) + entry_count * sizeof(I081Entry) + sizeof(I081Footer);

  std::vector<std::byte> buf(total_size, std::byte{0});

  // --- Fixed Part ---
  auto* fixed = reinterpret_cast<I081FixedPart*>(buf.data());

  // Header
  fixed->header.esc_code = 27;
  fixed->header.transmission_code = '2';
  fixed->header.message_kind = 'U';                        // U = Update
  encode_bcd(fixed->header.info_time, 6, 90000000000ULL);  // 09:00:00.000000
  encode_bcd(fixed->header.channel_id, 2, 1);
  encode_bcd(fixed->header.channel_seq, 5, 100);
  fixed->header.version_no = 0x01;
  encode_bcd(fixed->header.body_length, 2, static_cast<uint64_t>(total_size - 19));

  // Body
  std::memcpy(fixed->prod_id, prod_id, std::min(std::strlen(prod_id), size_t{20}));
  for (size_t i = std::strlen(prod_id); i < 20; ++i) {
    fixed->prod_id[i] = ' ';
  }

  encode_bcd(fixed->prod_msg_seq, 5, prod_msg_seq);
  encode_bcd(&fixed->no_md_entries, 1, entry_count);

  // --- Entries ---
  if (entry_count > 0) {
    auto* entry_ptr = reinterpret_cast<I081Entry*>(buf.data() + sizeof(I081FixedPart));
    for (size_t i = 0; i < entries.size(); ++i) {
      entry_ptr[i].md_update_action = entries[i].action;
      entry_ptr[i].md_entry_type = entries[i].type;
      entry_ptr[i].sign = entries[i].sign;
      encode_bcd(entry_ptr[i].md_entry_px, 5, entries[i].price);
      encode_bcd(entry_ptr[i].md_entry_size, 4, entries[i].size);
      encode_bcd(&entry_ptr[i].md_price_level, 1, entries[i].level);
    }
  }

  // --- Footer ---
  size_t footer_offset = sizeof(I081FixedPart) + entry_count * sizeof(I081Entry);
  auto* footer = reinterpret_cast<I081Footer*>(buf.data() + footer_offset);
  footer->check_sum = 'X';
  footer->terminal_code[0] = 0x0D;
  footer->terminal_code[1] = 0x0A;

  return buf;
}

}  // namespace

// ============================================================================
// 成功案例測試
// ============================================================================

TEST(ParserI081, ParseSingleNew) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec.flow_group = 1;
  spec_table.update(spec);

  // New: 買一檔 18500.00 x 10
  std::vector<UpdateEntry> entries = {
      {UpdateAction::New, EntryType::Bid, Sign::Positive, 1850000, 10, 1},
  };

  auto buf = build_i081_buffer(prod_id, 100, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  ASSERT_TRUE(update_opt.has_value());
  auto& update = *update_opt;

  EXPECT_EQ(update.entry_count, 1);
  EXPECT_EQ(update.entries[0].action, UpdateAction::New);
  EXPECT_EQ(update.entries[0].type, EntryType::Bid);
  EXPECT_EQ(update.entries[0].price, Price::from(18500.00));
  EXPECT_EQ(update.entries[0].size, Quantity::from(10));
  EXPECT_EQ(update.entries[0].level, 0);  // 0-based
}

TEST(ParserI081, ParseMultipleUpdates) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec_table.update(spec);

  // 多筆更新：New + Change + Delete
  std::vector<UpdateEntry> entries = {
      {UpdateAction::New, EntryType::Bid, Sign::Positive, 1850000, 10, 1},
      {UpdateAction::Change, EntryType::Ask, Sign::Positive, 1850100, 20, 1},
      {UpdateAction::Delete, EntryType::Bid, Sign::Positive, 0, 0, 2},
  };

  auto buf = build_i081_buffer(prod_id, 200, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  ASSERT_TRUE(update_opt.has_value());
  auto& update = *update_opt;

  EXPECT_EQ(update.entry_count, 3);
  EXPECT_EQ(update.entries[0].action, UpdateAction::New);
  EXPECT_EQ(update.entries[1].action, UpdateAction::Change);
  EXPECT_EQ(update.entries[2].action, UpdateAction::Delete);
}

TEST(ParserI081, ParseDerivedOverlay) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec_table.update(spec);

  // Overlay: 衍生買一檔
  std::vector<UpdateEntry> entries = {
      {UpdateAction::Overlay, EntryType::DerivedBid, Sign::Positive, 1849500, 100, 1},
  };

  auto buf = build_i081_buffer(prod_id, 300, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  ASSERT_TRUE(update_opt.has_value());
  auto& update = *update_opt;

  EXPECT_EQ(update.entry_count, 1);
  EXPECT_EQ(update.entries[0].action, UpdateAction::Overlay);
  EXPECT_EQ(update.entries[0].type, EntryType::DerivedBid);
  EXPECT_EQ(update.entries[0].price, Price::from(18495.00));
}

TEST(ParserI081, ParseNegativePrice) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec_table.update(spec);

  // 負價格（理論上期貨不會有，但測試 parser 正確性）
  std::vector<UpdateEntry> entries = {
      {UpdateAction::New, EntryType::Bid, Sign::Negative, 100, 10, 1},
  };

  auto buf = build_i081_buffer(prod_id, 400, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  ASSERT_TRUE(update_opt.has_value());
  auto& update = *update_opt;

  EXPECT_EQ(update.entries[0].price, Price::from(-1.00));
}

// ============================================================================
// 錯誤案例測試
// ============================================================================

TEST(ParserI081, BufferTooSmall) {
  ProductSpecTable spec_table;
  std::vector<std::byte> buf(10, std::byte{0});  // 太小

  auto update_opt = parse_i081(std::span(buf), spec_table);
  EXPECT_FALSE(update_opt.has_value());
}

TEST(ParserI081, UnknownProduct) {
  ProductSpecTable spec_table;  // 空的 spec_table

  std::vector<UpdateEntry> entries = {
      {UpdateAction::New, EntryType::Bid, Sign::Positive, 1850000, 10, 1},
  };

  auto buf = build_i081_buffer("UNKNOWN   ", 100, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  EXPECT_FALSE(update_opt.has_value());  // 未知商品
}

TEST(ParserI081, InvalidLevel) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec_table.update(spec);

  // Level = 6 (超出範圍)
  std::vector<UpdateEntry> entries = {
      {UpdateAction::New, EntryType::Bid, Sign::Positive, 1850000, 10, 6},
  };

  auto buf = build_i081_buffer(prod_id, 100, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  EXPECT_FALSE(update_opt.has_value());  // Level 超出範圍
}

TEST(ParserI081, MaxEntries) {
  ProductSpecTable spec_table;
  const char* prod_id = "TXF       ";

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), prod_id, 10);
  spec.decimal_locator = 2;
  spec_table.update(spec);

  // 12 筆更新（最大值）
  std::vector<UpdateEntry> entries;
  for (uint8_t i = 1; i <= 5; ++i) {
    entries.push_back({UpdateAction::New, EntryType::Bid, Sign::Positive, 1850000, 10, i});
    entries.push_back({UpdateAction::New, EntryType::Ask, Sign::Positive, 1850100, 10, i});
  }
  entries.push_back(
      {UpdateAction::Overlay, EntryType::DerivedBid, Sign::Positive, 1849500, 100, 1});
  entries.push_back(
      {UpdateAction::Overlay, EntryType::DerivedAsk, Sign::Positive, 1850500, 100, 1});

  auto buf = build_i081_buffer(prod_id, 500, entries);
  auto update_opt = parse_i081(std::span(buf), spec_table);

  ASSERT_TRUE(update_opt.has_value());
  EXPECT_EQ(update_opt->entry_count, 12);
}
