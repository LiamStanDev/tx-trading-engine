#ifndef ONYX_CORE_ORDER_BOOK_HPP
#define ONYX_CORE_ORDER_BOOK_HPP

#include "onyx/core/type.hpp"
#include "onyx/net/taifex/constant.hpp"
#include "onyx/net/taifex/message.hpp"

namespace onyx::core {

using namespace net::taifex;

using BookSide = std::array<PriceLevel, BOOK_DEPTH>;

/// @brief 五檔委託簿
class OrderBook {
 public:
  // ----------------------------------------------------------------------------
  // Constructor
  // ----------------------------------------------------------------------------
  OrderBook() noexcept = default;

  // ----------------------------------------------------------------------------
  // 快照與更新
  // ----------------------------------------------------------------------------

  /// @brief I083 快照重建委託簿
  void reset_from_snapshot(const BookSnapshot& snapshot) noexcept;

  /// @brief I081 委託簿更新
  void apply_update(const BookUpdate& update) noexcept;

  // ----------------------------------------------------------------------------
  // 狀態查詢
  // ----------------------------------------------------------------------------

  /// @brief 取得買檔
  ///
  /// @param level 檔位 (0-4)
  [[nodiscard]] const PriceLevel& bid(uint8_t level) const noexcept;

  /// @brief 取得賣檔
  ///
  /// @param level 檔位 (0-4)
  [[nodiscard]] const PriceLevel& ask(uint8_t level) const noexcept;

  /// @brief 取得衍生買一檔
  [[nodiscard]] const PriceLevel& derived_bid() const noexcept { return derived_bid_; }

  /// @brief 取得衍生賣一檔
  [[nodiscard]] const PriceLevel& derived_ask() const noexcept { return derived_ask_; }

 private:
  // ----------------------------------------------------------------------------
  // Data Members
  // ----------------------------------------------------------------------------
  BookSide bids_;             ///< 買檔
  BookSide asks_;             ///< 賣檔
  PriceLevel derived_bid_{};  ///< 衍生買一檔
  PriceLevel derived_ask_{};  ///< 衍生賣一檔

 private:
  void apply_new(BookSide& side, uint8_t level, Price price, Quantity size) noexcept;
  void apply_change(BookSide& side, uint8_t level, Quantity new_size) noexcept;
  void apply_delete(BookSide& side, uint8_t level) noexcept;
};

}  // namespace onyx::core

#endif
