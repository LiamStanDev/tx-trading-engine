#ifndef TX_TRADING_ENGINE_NET_TAIFEX_PARSER_HPP
#define TX_TRADING_ENGINE_NET_TAIFEX_PARSER_HPP

#include <arpa/inet.h>

#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

#include "tx/error.hpp"

namespace tx::net::taifex {

/// @brief 解析後的 Packet Header (Host Order)
struct ParsedPacketHeader {
  uint8_t esc_code;        ///< 固定值 0x1B
  uint8_t packet_version;  ///< 格式版本（e.g. 0x01）
  uint16_t packet_length;  ///< 整個封包長度（含Header）
  uint16_t msg_count;      ///< 訊息數量
  uint32_t pkt_seq_num;    ///< 封包序號
  uint16_t channel_id;     ///< 頻道編號
  uint32_t send_time;      ///< 送出時間 (HHMMSSuu)
};

/// @brief 解析後的 R06 Level
struct ParsedR06Level {
  int32_t price;
  uint32_t quantity;
  uint32_t order_count;
};

/// @brief 解析後的 R06 Snapshot
struct ParsedR06Snapshot {
  char prod_id[20];  ///< 商品代碼
  uint8_t prod_status;
  uint32_t update_time;

  uint8_t bid_level_cnt;
  ParsedR06Level bid_levels[5];

  uint8_t ask_level_cnt;
  ParsedR06Level ask_levels[5];

  int32_t last_price;
  uint32_t last_qty;
  uint32_t total_volume;
};

/// @brief 解析後的 R02 Trade
struct ParsedR02Trade {
  char prod_id[20];
  int32_t match_price;
  uint32_t match_qty;
  uint32_t total_volume;
  uint64_t match_time;
  uint8_t side;
};

/// @brief 解析 Packet Header
[[nodiscard]] Result<ParsedPacketHeader> parse_packet_header(
    std::span<const std::byte> data) noexcept;

/// @brief R06 Snapshot 訊息
[[nodiscard]] Result<ParsedR06Snapshot> parse_r06_snapshot(
    std::span<const std::byte> data) noexcept;

/// @brief 解析 R02 Trade 訊息
[[nodiscard]]
Result<ParsedR02Trade> parse_r02_trade(
    std::span<const std::byte> data) noexcept;

}  // namespace tx::net::taifex

#endif
