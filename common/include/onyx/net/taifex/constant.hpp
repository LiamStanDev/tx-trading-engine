#ifndef ONYX_NET_TAIFEX_CONSTANT_HPP
#define ONYX_NET_TAIFEX_CONSTANT_HPP

#include <cstddef>

namespace onyx::net::taifex {

enum class UpdateAction : char {
  New = '0',     ///< 新增
  Change = '1',  ///< 修改
  Delete = '2',  ///< 刪除
  Overlay = '5'  ///< 覆蓋
};

enum class EntryType : char {
  Bid = '0',         ///< 買檔
  Ask = '1',         ///< 賣檔
  DerivedBid = 'E',  ///< 衍生買一檔
  DerivedAsk = 'F',  ///< 衍生賣一檔
};

enum class Sign : char {
  Positive = '0',  ///< 正價格
  Negative = '-'   ///< 負價格
};

enum class CalculatedFlag : char {
  Real = '0',      ///< 開盤
  Simulated = '1'  ///< 試撮
};

constexpr size_t BOOK_DEPTH = 5;  ///< 委託簿檔位深度

constexpr size_t PROD_ID_LEN = 20;    ///< 商品代號
constexpr size_t PROD_ID_S_LEN = 10;  ///< 商品代號(短)

constexpr size_t BCD_TIME_LEN = 6;   ///< PACK BCD 時間長度
constexpr size_t BCD_PRICE_LEN = 5;  ///< PACK BCD 價格長度

}  // namespace onyx::net::taifex

#endif
