#ifndef TX_TRADING_ENGINE_NET_TAIFEX_WIRE_FORMAT_HPP
#define TX_TRADING_ENGINE_NET_TAIFEX_WIRE_FORMAT_HPP

#include <cstdint>

namespace tx::net::taifex {

// =====================================
// UDP 封包 Header
// =====================================
#pragma pack(push, 1)
struct PacketHeader {
  uint8_t esc_code;        ///< 固定值 0x1B
  uint8_t packet_version;  ///< 格式版本（e.g. 0x01）
  uint16_t packet_length;  ///< 整個封包長度（含Header）
  uint16_t msg_count;      ///< 訊息數量
  uint32_t pkt_seq_num;    ///< 封包序號
  uint16_t channel_id;     ///< 頻道編號
  uint32_t send_time;      ///< 送出時間 (HHMMSSuu)
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes");
static_assert(alignof(PacketHeader) == 1, "PacketHeader must be packet");

// =====================================
// 每個訊息的開頭
// =====================================
#pragma pack(push, 1)
struct MessageHeader {
  uint16_t msg_length;  ///< 單一訊息長度
  char msg_kind;        ///< 'R' (Real-time)
  char msg_type;        ///< '6' (R06) 或者 '2' (R02)
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 4, "MessageHeader must be 4 bytes");

// =====================================
// R06 單檔價格資訊
// =====================================
#pragma pack(push, 1)
struct R06Level {
  int32_t price;         ///< 委託價格 (單位一商品tick而異)
  uint32_t quantity;     ///< 委託口數
  uint32_t order_count;  ///< 該價位總委託口數
};
#pragma pack(pop)

static_assert(sizeof(R06Level) == 12, "R06Level must be 12 bytes");

// =====================================
// R06 五檔行情
// =====================================
#pragma pack(push, 1)
struct R06SnapshotWire {
  MessageHeader header;  ///< 4 bytes
  char prod_id[20];      ///< 商品代碼（左靠右補空白）
  uint8_t prod_status;   ///< 狀態碼（0:正常, 1:暫停, 2:收盤）
  uint32_t update_time;  ///< 交易所更新時間 (HHMMSSuu)
  // Bid
  uint8_t bid_level_cnt;    ///< 買進揭示檔數（通常 5）
  R06Level bid_entries[5];  ///< 5 檔買進資訊 (60 bytes)
  // Ask
  uint8_t ask_level_cnt;    ///< 賣出揭示檔數（通常 5）
  R06Level ask_entries[5];  ///< 5 檔賣出資訊 (60 bytes)
  // Summary
  int32_t last_price;     ///< 最新成交價
  uint32_t last_qty;      ///< 最新成交量
  uint32_t total_volume;  ///< 當日累積成交總量
};
#pragma pack(pop)

static_assert(sizeof(R06SnapshotWire) == 163,
              "R06SnapshotWire must be 163 bytes");

// =====================================
// R02 成交訊息
// =====================================
#pragma pack(push, 1)
struct R02TradeWire {
  MessageHeader header;   ///< 4 bytes
  char prod_id[20];       ///< 商品代碼
  int32_t match_price;    ///< 成交價格
  uint32_t match_qty;     ///< 成交數量
  uint32_t total_volume;  ///< 當日累積成交總量
  uint64_t match_time;    ///< HHMMSSuuuuuu（微秒精度時間戳）
  uint8_t side;           ///< 1:買方主動, 2:賣方主動, 0:不詳
};
#pragma pack(pop)

static_assert(sizeof(R02TradeWire) == 45, "R02TradeWire must be 45 bytes");
}  // namespace tx::net::taifex

#endif
