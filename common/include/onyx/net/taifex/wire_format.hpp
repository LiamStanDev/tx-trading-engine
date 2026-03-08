#ifndef ONYX_NET_TAIFEX_WIRE_FORMAT_HPP
#define ONYX_NET_TAIFEX_WIRE_FORMAT_HPP

#include <stdint.h>

#include <cstdint>

namespace onyx::net::taifex {

/// @brief 行情訊息共用檔頭
struct __attribute__((packed)) StandardHeader {
  char esc_code;           ///< ASCII 27
  char transmission_code;  ///<
  char message_kind;       ///<
  uint8_t info_time[6];    ///< [PACK BCD], 時:分:秒:3位毫秒:3位微秒
  uint8_t channel_id[2];   ///< [PACK BCD]: 傳輸群組編號
  uint8_t channel_seq[5];  ///< [PACK BCD]: 傳輸群組訊息流水號
  uint8_t version_no;      ///< [PACK BCD]: 電文格式版本
  uint8_t body_length[2];  ///< [PACK BCD]: 電文長度
};

static_assert(sizeof(StandardHeader) == 19, "StandarHeader should be 19 bytes");

/// @brief I010 商品基本資訊(規格)
///
/// @details
/// | 欄位 | 格式 | 長度 | 說明 | 備註 |
/// |------|------|------|------|------|
/// | HEADER | | 19 | 共用檔頭 | |
/// | PROD-ID-S | X(10) | 10 | 商品代號 | 注意：10 bytes，非 20 |
/// | REFERENCE-PRICE | 9(9) | 5 | 參考價 PACK BCD | |
/// | PROD-KIND | X(1) | 1 | 契約種類 | I(指數)/R(利率)/B(債券)/C(商品)/S(股票)/E(匯率) |
/// | DECIMAL-LOCATOR | 9(1) | 1 | 價格小數位數 PACK BCD | 0-4 |
/// | STRIKE-PRICE-DECIMAL-LOCATOR | 9(1) | 1 | 履約價小數位數 | 期貨固定為 0 |
/// | BEGIN-DATE | 9(8) | 4 | 上市日期 YYYYMMDD | |
/// | END-DATE | 9(8) | 4 | 下市日期 YYYYMMDD | |
/// | FLOW-GROUP | 9(2) | 1 | 流程群組 PACK BCD | 決定開收盤時間 |
/// | DELIVERY-DATE | 9(8) | 4 | 最後結算日 YYYYMMDD | |
/// | DYNAMIC-BANDING | X(1) | 1 | 適用動態價格穩定 | Y/N |
/// | LIST-TYPE | X(1) | 1 | 型別註記 | S/W/空白 |
/// | SETTLE-DATE | 9(6) | 3 | 交割年月 YYYYMM | |
/// | CONTRACT-DATE | 9(8) | 4 | 交割年月日 YYYYMMDD | |
/// | CHECK-SUM | X(1) | 1 | 檢核碼 | |
/// | TERMINAL-CODE | X(2) | 2 | 0D 0A | |
struct __attribute__((packed)) I010 {
  StandardHeader header;
  char prod_id_s[10];
  uint8_t reference_price[5];
  char prod_kind;
  uint8_t decimal_locator;
  uint8_t strike_price_decimal_locator;
  uint8_t begin_date[4];
  uint8_t end_date[4];
  uint8_t flow_group;
  uint8_t delivery_date[4];
  char dynamic_banding;
  char list_type;
  uint8_t settle_date[3];
  uint8_t contract_date[4];
  char check_sum;
  char terminal_code[2];
};

static_assert(sizeof(I010) == 62, "I010FixedPart should be 62 bytes");

/// @brief I204 成交價量揭示
struct __attribute__((packed)) I024FixedPart {
  StandardHeader header;
  char prod_id[20];              ///< X(20)
  uint8_t prod_msg_seq[5];       ///< [PACK BCD] 商品行情訊息流水號
  char calculated_flag;          ///< 試撮價格註記，1 表示試撮
  uint8_t match_time[6];         ///< [PACK BCD] 成交時間 (時:分:秒:3位毫秒:3位微秒)
  char sign;                     ///< 價格正負號
  uint8_t first_match_price[5];  ///< [PACK BCD] 第一成交價
  uint8_t first_match_qty[4];    ///< [PACK BCD] 第一成交量
  char match_display_item;       ///< 成交揭示項目註記:
                                 ///< Bit7 = 1 表示揭示第一個封包, = 0
                                 ///< 表示延續上一個封包
};

static_assert(sizeof(I024FixedPart) == 62, "I024FixedPart should be 62");

struct __attribute__((packed)) I024MatchEntry {
  char sign;               ///< 價格正負號
  uint8_t match_price[5];  ///< [PACK BCD] 成交價格
  uint8_t match_qty[2];    ///< [PACK BCD] 成交數量
};

static_assert(sizeof(I024MatchEntry) == 8, "I024MatchEntry should be 8");

struct __attribute__((packed)) I024Footer {
  uint8_t match_total_qty[4];  ///< [PACK BCD] 累計成交數量
  uint8_t match_buy_cnt[4];    ///< [PACK BCD] 累計買進成交筆數
  uint8_t match_sell_cnt[4];   ///< [PACK BCD] 累計賣出成交筆數
  char check_sum;              ///< 檢核碼
  char terminal_code[2];       ///< HEXACODE : 0D 0A
};

static_assert(sizeof(I024Footer) == 15, "I024Footer should be 8");

}  // namespace onyx::net::taifex
#endif
