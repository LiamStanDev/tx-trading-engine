#include "onyx/net/taifex/parser.hpp"

#include <cstring>
#include <optional>
#include <span>

#include "onyx/core/type.hpp"
#include "onyx/net/taifex/codec.hpp"
#include "onyx/net/taifex/constant.hpp"
#include "onyx/net/taifex/message.hpp"
#include "onyx/net/taifex/wire_format.hpp"

namespace onyx::net::taifex {

std::optional<ProductSpec> parse_i010(std::span<const std::byte> buffer) noexcept {
  // 驗証最小長度
  constexpr size_t MIN_SIZE = sizeof(I010);
  if (buffer.size() < MIN_SIZE) {
    return std::nullopt;
  }

  const I010* i010 = reinterpret_cast<const I010*>(buffer.data());

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), i010->prod_id_s, spec.prod_id.size());

  spec.decimal_locator = unpack_bcd(i010->decimal_locator);
  spec.flow_group = unpack_bcd(i010->flow_group);

  return spec;
}

std::optional<TradePacket> parse_i024(std::span<const std::byte> buffer,
                                      const ProductSpecTable& spec_table) noexcept {
  // 驗証最小長度
  constexpr size_t MIN_SIZE = sizeof(I024FixedPart) + sizeof(I024Footer);
  if (buffer.size() < MIN_SIZE) {
    return std::nullopt;
  }

  const I024FixedPart* fixed = reinterpret_cast<const I024FixedPart*>(buffer.data());

  // 計算實際長度並驗証
  uint8_t match_display_count = fixed->match_display_item & 0x7F;  // 看 Bit(0-6)
  if (match_display_count > TradePacket::MAX_MATCH_SIZE) [[unlikely]] {
    return std::nullopt;
  }

  size_t expected_size = MIN_SIZE + match_display_count * sizeof(I024MatchEntry);

  if (buffer.size() < expected_size) {
    return std::nullopt;
  }

  // 取得 footer
  size_t footer_offset = sizeof(I024FixedPart) + match_display_count * sizeof(I024MatchEntry);

  const I024Footer* footer = reinterpret_cast<const I024Footer*>(buffer.data() + footer_offset);

  TradePacket packet;

  // --- 解析 packet ---
  std::memcpy(packet.prod_id.data(), fixed->prod_id, packet.prod_id.size());
  // 查詢使用 10 碼的
  ProdIdKey prod_id_s;
  std::memcpy(prod_id_s.data(), packet.prod_id.data(), prod_id_s.size());

  auto decimal_locator_opt = spec_table.get_decimal_locator(prod_id_s);
  if (!decimal_locator_opt) [[unlikely]] {
    return std::nullopt;
  }

  packet.prod_msg_seq = unpack_bcd(std::span(fixed->prod_msg_seq));
  packet.match_time = unpack_bcd_time(fixed->match_time);

  packet.is_simulated = fixed->calculated_flag == CalculatedFlag::Simulated;
  packet.total_qty = unpack_bcd_qty(std::span(footer->match_total_qty));
  packet.buy_count = unpack_bcd(std::span(footer->match_buy_cnt));
  packet.sell_count = unpack_bcd(std::span(footer->match_sell_cnt));

  // --- 解析 matches ---
  packet.match_count = 1 + match_display_count;
  // 第一個會放在 packet 中
  TradeMatch first;
  bool is_negative = fixed->sign == Sign::Negative;
  first.price = unpack_bcd_price(fixed->first_match_price, *decimal_locator_opt, is_negative);
  first.qty = unpack_bcd_qty(std::span(fixed->first_match_qty));
  packet.matches[0] = first;

  // 其他會放在 entries 裡面
  const I024MatchEntry* entries =
      reinterpret_cast<const I024MatchEntry*>(buffer.data() + sizeof(I024FixedPart));
  for (uint8_t i = 0; i < match_display_count; ++i) {
    TradeMatch match;
    const I024MatchEntry& entry = entries[i];
    is_negative = entry.sign == Sign::Negative;
    match.price = unpack_bcd_price(entry.match_price, *decimal_locator_opt, is_negative);
    match.qty = unpack_bcd_qty(std::span(entry.match_qty));
    packet.matches[i + 1] = match;
  }

  return packet;
}

std::optional<BookSnapshot> parse_i083(std::span<const std::byte> buffer,
                                       const ProductSpecTable& spec_table) noexcept {
  constexpr size_t MIN_SIZE = sizeof(I083FixedPart) + sizeof(I083Footer);
  if (buffer.size() < MIN_SIZE) {
    return std::nullopt;
  }

  const I083FixedPart* fixed = reinterpret_cast<const I083FixedPart*>(buffer.data());
  uint8_t num_entries = unpack_bcd(fixed->no_md_entries);

  // 檢查 entries <= 買檔 (5) + 賣檔 (5) + 衍生買 (1) + 衍生賣 (1) = 12
  // num_entries 表示全部清空
  if (num_entries > 2 * BOOK_DEPTH + 2) {
    return std::nullopt;
  }

  // -- 解析委託簿快照 --
  BookSnapshot snapshot{};
  std::memcpy(snapshot.prod_id.data(), fixed->prod_id, snapshot.prod_id.size());
  snapshot.prod_msg_seq = unpack_bcd(std::span(fixed->prod_msg_seq));
  snapshot.is_simulated = fixed->calculated_flag == CalculatedFlag::Simulated;

  // 查詢小數位數
  ProdIdKey prod_id_s;
  std::memcpy(prod_id_s.data(), snapshot.prod_id.data(), prod_id_s.size());

  auto decimal_locator_opt = spec_table.get_decimal_locator(prod_id_s);
  if (!decimal_locator_opt) [[unlikely]] {
    return std::nullopt;
  }

  const I083Entry* entries =
      reinterpret_cast<const I083Entry*>(buffer.data() + sizeof(I083FixedPart));

  for (size_t i = 0; i < num_entries; ++i) {
    const I083Entry& entry = entries[i];
    uint8_t level = unpack_bcd(entry.md_price_level);

    if (level < 1 || level > BOOK_DEPTH) {
      return std::nullopt;
    }

    bool is_negative = entry.sign == Sign::Negative;
    Price price = unpack_bcd_price(entry.md_entry_px, *decimal_locator_opt, is_negative);
    Quantity quntity = unpack_bcd_qty(std::span(entry.md_entry_size));
    PriceLevel price_level{price, quntity};

    switch (entry.md_entry_type) {
      case EntryType::Bid:  // 買
        snapshot.bids[level - 1] = price_level;
        break;
      case EntryType::Ask:  // 賣
        snapshot.asks[level - 1] = price_level;
        break;
      case EntryType::DerivedBid:  // 衍生買
        if (level != 1) [[unlikely]] {
          return std::nullopt;
        }
        snapshot.derived_bid = price_level;
        break;
      case EntryType::DerivedAsk:  // 衍生賣
        if (level != 1) [[unlikely]] {
          return std::nullopt;
        }
        snapshot.derived_ask = price_level;
        break;
      default:
        return std::nullopt;
    }
  }

  return snapshot;
}

std::optional<BookUpdate> parse_i081(std::span<const std::byte> buffer,
                                     const ProductSpecTable& spec_table) noexcept {
  // 因為沒有 Entry 這個封包就沒有意義，所以這邊設定至少要有一個
  constexpr size_t MIN_SIZE = sizeof(I081FixedPart) + sizeof(I081Entry) + sizeof(I081Footer);

  if (buffer.size() < MIN_SIZE) {
    return std::nullopt;
  }

  const I081FixedPart* fixed = reinterpret_cast<const I081FixedPart*>(buffer.data());
  uint8_t num_entries = unpack_bcd(fixed->no_md_entries);

  if (num_entries == 0 || num_entries > BookUpdate::MAX_ENTRIES) {
    return std::nullopt;
  }

  // -- 解析委託簿更新 --
  BookUpdate update{};
  update.entry_count = num_entries;
  std::memcpy(update.prod_id.data(), fixed->prod_id, update.prod_id.size());
  update.prod_msg_seq = unpack_bcd(std::span(fixed->prod_msg_seq));

  // 查詢小數點位數
  ProdIdKey prod_id_s;
  std::memcpy(prod_id_s.data(), update.prod_id.data(), prod_id_s.size());

  auto decimal_locator_opt = spec_table.get_decimal_locator(prod_id_s);
  if (!decimal_locator_opt) [[unlikely]] {
    return std::nullopt;
  }

  const I081Entry* entries =
      reinterpret_cast<const I081Entry*>(buffer.data() + sizeof(I081FixedPart));
  for (size_t i = 0; i < num_entries; ++i) {
    const I081Entry& entry = entries[i];

    uint8_t level = unpack_bcd(entry.md_price_level);

    if (level == 0 || level > BOOK_DEPTH) [[unlikely]] {
      return std::nullopt;
    }

    bool is_negative = entry.sign == Sign::Negative;
    update.entries[i] = {
        .action = entry.md_update_action,
        .type = entry.md_entry_type,
        .price = unpack_bcd_price(entry.md_entry_px, *decimal_locator_opt, is_negative),
        .size = unpack_bcd_qty(std::span(entry.md_entry_size)),
        .level = static_cast<uint8_t>(level - 1),
    };
  }

  return update;
}

}  // namespace onyx::net::taifex
