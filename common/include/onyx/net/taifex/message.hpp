#ifndef ONYX_NET_TAIFEX_MESSAGE_HPP
#define ONYX_NET_TAIFEX_MESSAGE_HPP

#include <stdint.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <unordered_map>

#include "onyx/core/type.hpp"
#include "onyx/net/taifex/constant.hpp"

namespace onyx::net::taifex {

using namespace onyx::core;

using ProdIdKey = std::array<char, PROD_ID_S_LEN>;

/// @brief 商品基本資料
struct ProductSpec {
  ProdIdKey prod_id;        ///< 商品代號（10 bytes，右側填空白）
  uint8_t flow_group;       ///< 流程群組（1-14）
  uint8_t decimal_locator;  ///< 價格小數位數（0-4）
};

struct ProdIdHash {
  [[nodiscard]] size_t operator()(const ProdIdKey& key) const noexcept {
    // 前 8 bytes
    uint64_t part1;
    std::memcpy(&part1, key.data(), 8);

    // 後 2 bytes
    uint16_t part2;
    std::memcpy(&part2, key.data() + 8, 2);

    // 使用魔法常數進行乘法混淆，用來將連續或相似的輸入值徹底打散，避免太接近導致 HashMap
    // 會放在同一個格子
    size_t h1 = static_cast<size_t>(part1) * 0x9e3779b97f4a7c15ULL;  // 黃金分割率
    size_t h2 = static_cast<size_t>(part2) * 0xc6a4a7935bd1e995ULL;  // MurmurHash64A 混合數

    // 為什麼不直接 h1 ^ h2？
    // 如果 h1 和 h2 剛好算出相同的值，XOR 會變成 0，導致碰撞。
    // 加上常數 (0x9e3779b9) 以及左移 (<< 6)、右移 (>> 2)，
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};

/// @brief 商品規格表
class ProductSpecTable {
 public:
  /// @brief 更新或新增商品規格
  void update(const ProductSpec& spec) noexcept;

  /// @brief 查詢商品小數位數
  ///
  /// @param prod_id 10 bytes 格式的商品代碼
  /// @return 若商品不存在返回 nullopt
  std::optional<uint8_t> get_decimal_locator(ProdIdKey& prod_id) const noexcept;

 private:
  std::unordered_map<ProdIdKey, ProductSpec, ProdIdHash> table_;
};

/// @brief 成交價量
struct TradeMatch {
  Price price;   ///< 成交價
  Quantity qty;  ///< 成交量
};

/// @brief 成交信息
struct TradePacket {
  static inline constexpr size_t MAX_MATCH_SIZE = 71;

  std::array<char, PROD_ID_LEN> prod_id;           ///< 商品代碼
  uint64_t prod_msg_seq;                           ///< 商品訊商序列號
  std::chrono::nanoseconds match_time;             ///< 成交時間
  bool is_simulated;                               ///< 是否為試撮
  Quantity total_qty;                              ///< 累積交易量
  uint64_t buy_count;                              ///< 累積買進筆數
  uint64_t sell_count;                             ///< 累積賣出筆數
  uint8_t match_count;                             ///< 成交價量筆數
  std::array<TradeMatch, MAX_MATCH_SIZE> matches;  ///< 成交價量
};

/// @brief 委託簿快照
struct BookSnapshot {
  std::array<char, PROD_ID_LEN> prod_id;
  uint64_t prod_msg_seq;
  bool is_simulated;            // true=試撮, false=開盤
  PriceLevel bids[BOOK_DEPTH];  // 買檔 1-5
  PriceLevel asks[BOOK_DEPTH];  // 賣檔 1-5
  PriceLevel derived_bid;       // 衍生買一檔（MD-ENTRY-TYPE='E'）
  PriceLevel derived_ask;       // 衍生賣一檔（MD-ENTRY-TYPE='F'）
};

/// @brief 委託簿更新 (單筆)
struct BookUpdateEntry {
  UpdateAction action;
  EntryType type;
  Price price;
  Quantity size;
  uint8_t level;  // 0-4 (0-based)
};

/// @brief 委託簿更新 (整體)
struct BookUpdate {
  static inline constexpr size_t MAX_ENTRIES =
      12;  // WARN: 有待確認 (目前是覺得買賣各五檔 + 衍生一檔 x 2)

  std::array<char, PROD_ID_LEN> prod_id;
  uint64_t prod_msg_seq;
  uint8_t entry_count;                               // 實際筆數
  std::array<BookUpdateEntry, MAX_ENTRIES> entries;  // 更新項目
};

}  // namespace onyx::net::taifex

#endif
