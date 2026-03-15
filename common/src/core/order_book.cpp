#include "onyx/core/order_book.hpp"

#include <cstdint>
#include <cstring>

#include "onyx/core/type.hpp"
#include "onyx/net/taifex/constant.hpp"
#include "onyx/net/taifex/message.hpp"

namespace onyx::core {

void OrderBook::reset_from_snapshot(const BookSnapshot& snapshot) noexcept {
  // NOTE: 試搓快照當前不太重要，目前只要開盤快照就好
  if (snapshot.is_calculated) [[unlikely]] {
    return;
  }
  // -- 清空 --
  std::fill(std::begin(bids_), std::end(bids_), PriceLevel{});
  std::fill(std::begin(asks_), std::end(asks_), PriceLevel{});
  derived_bid_ = {};
  derived_ask_ = {};

  // -- 填入快照 --
  std::memcpy(bids_.data(), snapshot.bids, sizeof(bids_));
  std::memcpy(asks_.data(), snapshot.asks, sizeof(asks_));
  derived_bid_ = snapshot.derived_bid;
  derived_ask_ = snapshot.derived_ask;
}

const PriceLevel& OrderBook::bid(uint8_t level) const noexcept { return bids_[level]; }

const PriceLevel& OrderBook::ask(uint8_t level) const noexcept { return asks_[level]; }

void OrderBook::apply_update(const BookUpdate& update) noexcept {
  for (uint8_t i = 0; i < update.entry_count; ++i) {
    const BookUpdateEntry& entry = update.entries[i];

    BookSide* side = nullptr;
    switch (entry.type) {
      case EntryType::Bid:
        side = &bids_;
        break;
      case EntryType::Ask:
        side = &asks_;
        break;
      case EntryType::DerivedBid:
        if (entry.action == UpdateAction::Overlay) {
          derived_bid_ = PriceLevel{entry.price, entry.size};
        }
        continue;
      case EntryType::DerivedAsk:
        if (entry.action == UpdateAction::Overlay) {
          derived_ask_ = PriceLevel{entry.price, entry.size};
        }
        continue;
      default:
        continue;
    }

    switch (entry.action) {
      case UpdateAction::New:
        apply_new(*side, entry.level, entry.price, entry.size);
        break;
      case UpdateAction::Change:
        apply_change(*side, entry.level, entry.size);
        break;
      case UpdateAction::Delete:
        apply_delete(*side, entry.level);
        break;
      case UpdateAction::Overlay:
        // Overlay 只用於衍生一檔，已在上面處理
        break;
    }
  }
}

void OrderBook::apply_new(BookSide& side, uint8_t level, Price new_price,
                          Quantity new_size) noexcept {
  if (level < side.size() - 1) {
    // 向上動一位: level + 1 位置被 level 覆蓋，共有 5 - (level + 1) 個
    std::memmove(&side[level + 1], &side[level], (side.size() - (level + 1)) * sizeof(PriceLevel));
  }

  // 插入新一筆
  side[level] = {new_price, new_size};
}

void OrderBook::apply_change(BookSide& side, uint8_t level, Quantity new_size) noexcept {
  side[level].size = new_size;
}

void OrderBook::apply_delete(BookSide& side, uint8_t level) noexcept {
  if (level < side.size() - 1) {
    std::memmove(&side[level], &side[level + 1], (side.size() - (level + 1)) * sizeof(PriceLevel));
  }

  // 最後一檔清空
  side[side.size() - 1] = {};
}

}  // namespace onyx::core
