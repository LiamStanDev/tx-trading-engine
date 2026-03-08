#ifndef ONYX_NET_TAIFEX_MESSAGE_HPP
#define ONYX_NET_TAIFEX_MESSAGE_HPP

#include <stdint.h>

#include <array>
#include <chrono>
#include <unordered_map>

#include "onyx/core/type.hpp"

namespace onyx::net::taifex {

using namespace onyx::core;

/// @brief I010 商品基本資料（應用層）
struct ProductSpec {
  std::array<char, 10> prod_id;  ///< 商品代號（10 bytes，右側填空白）
  uint8_t flow_group;            ///< 流程群組（1-14）
  uint8_t decimal_locator;       ///< 價格小數位數（0-4）
};

/// @brief 商品規格表
class ProductSpecTable {
 public:
  /// @brief 更新或新增商品規格
  void update(const ProductSpec& spec) noexcept;

  /// @brief 查詢商品小數位數
  ///
  /// @return 若商品不存在返回 nullopt
  std::optional<uint8_t> get_decimal_locator(std::string_view prod_id) const noexcept;

 private:
  std::unordered_map<std::string, ProductSpec> table_;
};

/// @brief I024 成交信息
struct Trade {
  std::array<char, 20> prod_id;         ///< 商品代碼
  uint64_t prod_msg_seq;                ///< 商品訊商序列號
  std::chrono::nanoseconds match_time;  ///< 成交時間
  Price price;                          ///< 成交價
  Quantity qty;                         ///< 成交量
  bool is_calculated;                   ///< 是否為試撮
  Quantity total_qty;                   ///< 累積交易量
  uint64_t buy_count;                   ///< 累積買進筆數
  uint64_t sell_count;                  ///< 累積賣出筆數
};

}  // namespace onyx::net::taifex

#endif
