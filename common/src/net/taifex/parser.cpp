#include "onyx/net/taifex/parser.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <span>

#include "onyx/core/type.hpp"
#include "onyx/net/taifex/codec.hpp"
#include "onyx/net/taifex/message.hpp"
#include "onyx/net/taifex/wire_format.hpp"

namespace onyx::net::taifex {

std::optional<ProductSpec> parse_i010(std::span<const std::byte> buffer) {
  // 驗証最小長度
  constexpr size_t MIN_SIZE = sizeof(I010);
  if (buffer.size() < MIN_SIZE) {
    return std::nullopt;
  }

  const I010* i010 = reinterpret_cast<const I010*>(buffer.data());

  ProductSpec spec;
  std::memcpy(spec.prod_id.data(), i010->prod_id_s, 10);

  spec.decimal_locator = static_cast<uint8_t>(unpack_bcd(std::span(&i010->decimal_locator, 1)));
  spec.flow_group = static_cast<uint8_t>(unpack_bcd(std::span(&i010->flow_group, 1)));

  return spec;
}

std::optional<TradePacket> parse_i024(std::span<const std::byte> buffer,
                                      const ProductSpecTable& spec_table) {
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
  // prod_id 為 10 bytes，若右側空白要 trim
  std::string_view prod_id_sv(fixed->prod_id, packet.prod_id.size());
  size_t last = prod_id_sv.find_last_not_of(' ');
  size_t len = (last == std::string_view::npos) ? 0 : last + 1;
  prod_id_sv = prod_id_sv.substr(0, len);
  std::copy_n(prod_id_sv.begin(), len, packet.prod_id.begin());
  if (len < 10) {
    std::fill(packet.prod_id.begin() + len, packet.prod_id.end(), 0);
  }

  auto decimal_locator_opt = spec_table.get_decimal_locator(prod_id_sv);
  if (!decimal_locator_opt) {
    return std::nullopt;
  }

  int decimal_places = static_cast<int>(*decimal_locator_opt);

  packet.prod_msg_seq = unpack_bcd(std::span(fixed->prod_msg_seq, 5));
  packet.match_time = unpack_bcd_time(std::span(fixed->match_time, 6));

  packet.is_calculated = fixed->calculated_flag == '1';
  packet.total_qty =
      Quantity::from_value(static_cast<int64_t>(unpack_bcd(std::span(footer->match_total_qty))));
  packet.buy_count = unpack_bcd(std::span(footer->match_buy_cnt));
  packet.sell_count = unpack_bcd(std::span(footer->match_sell_cnt));

  // --- 解析 matches ---
  packet.match_count = 1 + match_display_count;
  // 第一個會放在 packet 中
  TradeMatch first;
  first.price =
      unpack_bcd_price(std::span(fixed->first_match_price, 5), decimal_places, fixed->sign);
  first.qty =
      Quantity::from_value(static_cast<int64_t>(unpack_bcd(std::span(fixed->first_match_qty, 4))));
  packet.matches[0] = first;

  // 其他會放在 entries 裡面
  const I024MatchEntry* entries =
      reinterpret_cast<const I024MatchEntry*>(buffer.data() + sizeof(I024FixedPart));
  for (uint8_t i = 0; i < match_display_count; ++i) {
    TradeMatch match;
    const I024MatchEntry& entry = entries[i];
    match.price = unpack_bcd_price(std::span(entry.match_price, 5), decimal_places, entry.sign);
    match.qty =
        Quantity::from_value(static_cast<int64_t>(unpack_bcd(std::span(entry.match_qty, 2))));
    packet.matches[i + 1] = match;
  }

  return packet;
}

}  // namespace onyx::net::taifex
