#include "tx/net/taifex/parser.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstring>
#include <expected>
#include <system_error>

#include "tx/net/taifex/error.hpp"
#include "tx/net/taifex/wire_format.hpp"

namespace tx::net::taifex {

/// @brief 解析 Packet Header
[[nodiscard]] Result<ParsedPacketHeader> parse_packet_header(
    std::span<const std::byte> data) noexcept {
  if (data.size() < sizeof(PacketHeader)) {
    return std::unexpected(parse_errc::buffer_too_small);
  }

  const auto* wire = reinterpret_cast<const PacketHeader*>(data.data());

  if (wire->esc_code != 0x1B) [[unlikely]] {
    return std::unexpected(parse_errc::invalid_esc_code);
  }

  ParsedPacketHeader parsed{.esc_code = wire->esc_code,
                            .packet_version = wire->packet_version,
                            .packet_length = ntohs(wire->packet_length),
                            .msg_count = ntohs(wire->msg_count),
                            .pkt_seq_num = ntohl(wire->pkt_seq_num),
                            .channel_id = ntohs(wire->channel_id),
                            .send_time = ntohl(wire->send_time)};

  if (parsed.msg_count == 0 || parsed.msg_count > 100) [[unlikely]] {
    return std::unexpected(parse_errc::invalid_msg_count);
  }

  if (parsed.packet_length < sizeof(PacketHeader) ||
      parsed.packet_length > data.size()) {
    return std::unexpected(parse_errc::invalid_packet_length);
  }

  return parsed;
}

/// @brief 解析 R06Level
ParsedR06Level convert_level(const R06Level& wire) noexcept {
  return ParsedR06Level{
      .price = static_cast<int32_t>(ntohl(static_cast<uint32_t>(wire.price))),
      .quantity = ntohl(wire.quantity),
      .order_count = ntohl(wire.order_count)};
}

/// @brief R06 Snapshot 訊息
[[nodiscard]] Result<ParsedR06Snapshot> parse_r06_snapshot(
    std::span<const std::byte> data) noexcept {
  if (data.size() < sizeof(R06SnapshotWire)) [[unlikely]] {
    return std::unexpected(parse_errc::buffer_too_small);
  }

  const auto* wire = reinterpret_cast<const R06SnapshotWire*>(data.data());

  if (wire->header.msg_kind != 'R') {
    return std::unexpected(parse_errc::invalid_msg_kind);
  }
  if (wire->header.msg_type != '6') {
    return std::unexpected(parse_errc::invalid_msg_type);
  }

  uint16_t msg_length = ntohs(wire->header.msg_length);
  if (msg_length != sizeof(R06SnapshotWire)) {
    return std::unexpected(parse_errc::invalid_msg_length);
  }

  ParsedR06Snapshot parsed{};

  std::memcpy(parsed.prod_id, wire->prod_id,
              20);  // Note 不能使用 sizeof 會退化
  parsed.prod_status = wire->prod_status;
  parsed.update_time = ntohl(wire->update_time);

  parsed.bid_level_cnt = wire->bid_level_cnt;
  for (uint8_t i = 0; i < 5; ++i) {
    parsed.bid_levels[i] = convert_level(wire->bid_entries[i]);
  }

  parsed.ask_level_cnt = wire->ask_level_cnt;
  for (uint8_t i = 0; i < 5; ++i) {
    parsed.ask_levels[i] = convert_level(wire->ask_entries[i]);
  }

  parsed.last_price =
      static_cast<int32_t>(ntohl(static_cast<uint32_t>(wire->last_price)));
  parsed.last_qty = ntohl(wire->last_qty);
  parsed.total_volume = ntohl(wire->total_volume);

  return parsed;
}

/// @brief 解析 R02 Trade 訊息
[[nodiscard]]
Result<ParsedR02Trade> parse_r02_trade(
    std::span<const std::byte> data) noexcept {
  if (data.size() < sizeof(R02TradeWire)) {
    return std::unexpected(make_error_code(parse_errc::buffer_too_small));
  }

  const auto* wire = reinterpret_cast<const R02TradeWire*>(data.data());

  if (wire->header.msg_kind != 'R' || wire->header.msg_type != '2')
      [[unlikely]] {
    return std::unexpected(make_error_code(parse_errc::invalid_msg_type));
  }

  // 轉換數據
  ParsedR02Trade parsed{};
  std::memcpy(parsed.prod_id, wire->prod_id, 20);

  parsed.match_price =
      static_cast<int32_t>(ntohl(static_cast<uint32_t>(wire->match_price)));
  parsed.match_qty = ntohl(wire->match_qty);
  parsed.total_volume = ntohl(wire->total_volume);

  parsed.match_time = be64toh(wire->match_time);
  parsed.side = wire->side;

  return parsed;
}

}  // namespace tx::net::taifex
