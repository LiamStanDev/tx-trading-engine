#ifndef ONYX_NET_TAIFEX_WIRE_FORMAT_HPP
#define ONYX_NET_TAIFEX_WIRE_FORMAT_HPP

#include <stdint.h>

#include <cstdint>

#include "onyx/net/taifex/constant.hpp"

namespace onyx::net::taifex {

/// @brief 行情訊息共用檔頭
struct __attribute__((packed)) StandardHeader {
  char esc_code;                    ///< ASCII 27
  char transmission_code;           ///<
  char message_kind;                ///<
  uint8_t info_time[BCD_TIME_LEN];  ///< [PACK BCD], 時:分:秒:3位毫秒:3位微秒
  uint8_t channel_id[2];            ///< [PACK BCD]: 傳輸群組編號
  uint8_t channel_seq[5];           ///< [PACK BCD]: 傳輸群組訊息流水號
  uint8_t version_no;               ///< [PACK BCD]: 電文格式版本
  uint8_t body_length[2];           ///< [PACK BCD]: 電文長度
};

static_assert(sizeof(StandardHeader) == 19, "StandarHeader should be 19 bytes");

// ----------------------------------------------------------------------------
// I010 商品基本資訊(規格)
// ----------------------------------------------------------------------------

struct __attribute__((packed)) I010 {
  StandardHeader header;
  char prod_id_s[PROD_ID_S_LEN];  ///< 商品代號，注意：10 bytes，非 20
  uint8_t reference_price[5];     ///< [PACK BCD] 參考價
  char prod_kind;                 ///< 契約種類: I(指數)/R(利率)/B(債券)/C(商品)/S(股票)/E(匯率)
  uint8_t decimal_locator;        ///< [PACK BCD] 價格小數位數
  uint8_t strike_price_decimal_locator;  ///< [PACK BCD] 履約價小數位數: 期貨固定為 0
  uint8_t begin_date[4];                 ///< [PACK BCD] 上市日期 YYYYMMDD
  uint8_t end_date[4];                   ///< [PACK BCD]下市日期 YYYYMMDD
  uint8_t flow_group;                    ///< [PACK BCD] 流程群組: 決定開收盤時間, 有 14 種
  uint8_t delivery_date[4];              ///< [PACK BCD] 最後結算日 YYYYMMDD
  char dynamic_banding;                  ///< 適用動態價格穩定 (Y/N)
  char list_type;                        ///< 型別註記，S: 標準選擇權, W: 周選擇權, 空白: 期貨
  uint8_t settle_date[3];                ///< [PACK BCD] 交割年月 YYYYMM
  uint8_t contract_date[4];              ///< [PACK BCD] 交割年月日 YYYYMMDD (選擇權)，期貨空白
  char check_sum;                        ///< 檢核碼
  char terminal_code[2];                 ///< HEXACODE : 0D 0A
};

static_assert(sizeof(I010) == 62, "I010FixedPart should be 62 bytes");

/// @brief I204 成交價量揭示
struct __attribute__((packed)) I024FixedPart {
  StandardHeader header;
  char prod_id[PROD_ID_LEN];                 ///< X(20)
  uint8_t prod_msg_seq[5];                   ///< [PACK BCD] 商品行情訊息流水號
  CalculatedFlag calculated_flag;            ///< 試撮價格註記，1 表示試撮
  uint8_t match_time[BCD_TIME_LEN];          ///< [PACK BCD] 成交時間 (時:分:秒:3位毫秒:3位微秒)
  Sign sign;                                 ///< 價格正負號
  uint8_t first_match_price[BCD_PRICE_LEN];  ///< [PACK BCD] 第一成交價
  uint8_t first_match_qty[4];                ///< [PACK BCD] 第一成交量
  char match_display_item;                   ///< 成交揭示項目註記:
                                             ///< Bit7 = 1 表示揭示第一個封包, = 0
                                             ///< 表示延續上一個封包
};

static_assert(sizeof(I024FixedPart) == 62, "I024FixedPart should be 62");

struct __attribute__((packed)) I024MatchEntry {
  Sign sign;                           ///< 價格正負號
  uint8_t match_price[BCD_PRICE_LEN];  ///< [PACK BCD] 成交價格
  uint8_t match_qty[2];                ///< [PACK BCD] 成交數量
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

// ----------------------------------------------------------------------------
// I083 委託簿快照
// ----------------------------------------------------------------------------

struct __attribute__((packed)) I083FixedPart {
  StandardHeader header;
  char prod_id[PROD_ID_LEN];       ///< X(20)
  uint8_t prod_msg_seq[5];         ///< [PACK BCD] 商品行情訊息流水號
  CalculatedFlag calculated_flag;  ///< 試撮價格註記，1 表式撮示
  uint8_t no_md_entries;           ///< [PACK BCD] 變更檔數
};

static_assert(sizeof(I083FixedPart) == 46, "I083FixedPart should be 46");

struct __attribute__((packed)) I083Entry {
  EntryType md_entry_type;             ///< 行情種類: 0: 買, 1: 賣, E: 衍生買, F: 衍生賣
  Sign sign;                           ///< 價格正負號
  uint8_t md_entry_px[BCD_PRICE_LEN];  ///< [PACK BCD] 行情價格
  uint8_t md_entry_size[4];            ///< [PACK BCD] 價格數量
  uint8_t md_price_level;              ///< [PACK BCD] 價格檔位
};

static_assert(sizeof(I083Entry) == 12, "I083Entry should be 12");

struct __attribute__((packed)) I083Footer {
  char check_sum;         ///< 檢核碼
  char terminal_code[2];  ///< HEXACODE : 0D 0A
};

static_assert(sizeof(I083Footer) == 3, "I083Footer should be 3");

// ----------------------------------------------------------------------------
// I081 委託揭示
// ----------------------------------------------------------------------------

struct __attribute__((packed)) I081FixedPart {
  StandardHeader header;
  char prod_id[PROD_ID_LEN];  ///< X(20)
  uint8_t prod_msg_seq[5];    ///< [PACK BCD] 商品行情訊息流水號
  uint8_t no_md_entries;      ///< [PACK BCD] 變更檔數
};

static_assert(sizeof(I081FixedPart) == 45, "I081FixedPart should be 45");

struct __attribute__((packed)) I081Entry {
  UpdateAction md_update_action;       ///< 行情揭示方式
  EntryType md_entry_type;             ///< 行情種類
  Sign sign;                           ///< 價格正負號
  uint8_t md_entry_px[BCD_PRICE_LEN];  ///< [PACK BCD] 行情價格
  uint8_t md_entry_size[4];            ///< [PACK BCD] 價格數量
  uint8_t md_price_level;              ///< [PACK BCD] 價格檔位
};

static_assert(sizeof(I081Entry) == 13, "I081Entry should be 13");

struct __attribute__((packed)) I081Footer {
  char check_sum;         ///< 檢核碼
  char terminal_code[2];  ///< HEXACODE : 0D 0A
};

static_assert(sizeof(I081Footer) == 3, "I081FixedPart should be 3");

}  // namespace onyx::net::taifex
#endif
