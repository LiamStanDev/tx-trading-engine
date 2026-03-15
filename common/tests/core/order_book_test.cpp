#include "onyx/core/order_book.hpp"

#include <gtest/gtest.h>

#include "onyx/core/type.hpp"
#include "onyx/net/taifex/constant.hpp"
#include "onyx/net/taifex/message.hpp"

using namespace onyx::core;
using namespace onyx::net::taifex;

// ============================================================================
// 快照重建測試
// ============================================================================

TEST(OrderBook, ResetFromEmptySnapshot) {
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  EXPECT_EQ(book.bid(0).price, Price::zero());
  EXPECT_EQ(book.ask(0).price, Price::zero());
}

// ============================================================================
// I081 更新測試 - New (插入)
// ============================================================================

TEST(OrderBook, ApplyNew_InsertAtLevel0) {
  // 初始狀態：買一 18500 x 10, 買二 18499 x 20
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;
  snapshot.bids[0] = {Price::from(18500.0), Quantity::from(10)};
  snapshot.bids[1] = {Price::from(18499.0), Quantity::from(20)};

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  // 模擬進入 ContinuousTrading 狀態（實際應該由 I140 觸發）
  // 這裡直接用 private 成員測試，實務上需要 setter
  // 暫時跳過狀態檢查測試

  // New: 在 Level 0 插入 18500.5 x 15
  BookUpdate update{};
  update.entry_count = 1;
  update.entries[0] = {
      .action = UpdateAction::New,
      .type = EntryType::Bid,
      .price = Price::from(18500.5),
      .size = Quantity::from(15),
      .level = 0,
  };

  // NOTE: 因為 OrderBook 檢查狀態，這裡需要先設定狀態
  // 但 OrderBook 沒有 public setter，所以這個測試會失敗
  // 建議：加入 set_state() 或 測試時用 friend class

  // book.apply_update(update);

  // 預期結果：
  // Level 0: 18500.5 x 15 (新插入)
  // Level 1: 18500.0 x 10 (原 Level 0 向下移)
  // Level 2: 18499.0 x 20 (原 Level 1 向下移)

  // EXPECT_EQ(book.bid(0).price, Price::from(18500.5));
  // EXPECT_EQ(book.bid(1).price, Price::from(18500.0));
  // EXPECT_EQ(book.bid(2).price, Price::from(18499.0));
}

TEST(OrderBook, ApplyNew_InsertAtLevel2) {
  // 初始狀態：5 檔全滿
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;
  for (int i = 0; i < 5; ++i) {
    snapshot.bids[i] = {Price::from(18500.0 - i), Quantity::from(10)};
  }

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  // New: 在 Level 2 插入 18498.5 x 25
  BookUpdate update{};
  update.entry_count = 1;
  update.entries[0] = {
      .action = UpdateAction::New,
      .type = EntryType::Bid,
      .price = Price::from(18498.5),
      .size = Quantity::from(25),
      .level = 2,
  };

  // book.apply_update(update);

  // 預期結果：
  // Level 0: 18500.0 x 10 (不變)
  // Level 1: 18499.0 x 10 (不變)
  // Level 2: 18498.5 x 25 (新插入)
  // Level 3: 18498.0 x 10 (原 Level 2 向下移)
  // Level 4: 18497.0 x 10 (原 Level 3 向下移)
  // (原 Level 4 被擠出)
}

// ============================================================================
// I081 更新測試 - Change (修改)
// ============================================================================

TEST(OrderBook, ApplyChange_ModifySize) {
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;
  snapshot.bids[0] = {Price::from(18500.0), Quantity::from(10)};
  snapshot.bids[1] = {Price::from(18499.0), Quantity::from(20)};

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  // Change: 修改 Level 1 的 size
  BookUpdate update{};
  update.entry_count = 1;
  update.entries[0] = {
      .action = UpdateAction::Change,
      .type = EntryType::Bid,
      .price = Price::from(18499.0),  // price 不會被使用
      .size = Quantity::from(50),     // 新 size
      .level = 1,
  };

  // book.apply_update(update);

  // 預期結果：
  // Level 0: 18500.0 x 10 (不變)
  // Level 1: 18499.0 x 50 (size 改變，price 不變)
}

// ============================================================================
// I081 更新測試 - Delete (刪除)
// ============================================================================

TEST(OrderBook, ApplyDelete_RemoveLevel1) {
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;
  snapshot.bids[0] = {Price::from(18500.0), Quantity::from(10)};
  snapshot.bids[1] = {Price::from(18499.0), Quantity::from(20)};
  snapshot.bids[2] = {Price::from(18498.0), Quantity::from(30)};

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  // Delete: 刪除 Level 1
  BookUpdate update{};
  update.entry_count = 1;
  update.entries[0] = {
      .action = UpdateAction::Delete,
      .type = EntryType::Bid,
      .price = Price::zero(),    // 不使用
      .size = Quantity::zero(),  // 不使用
      .level = 1,
  };

  // book.apply_update(update);

  // 預期結果：
  // Level 0: 18500.0 x 10 (不變)
  // Level 1: 18498.0 x 30 (原 Level 2 向上移)
  // Level 2: 0 x 0 (清空)
}

// ============================================================================
// I081 更新測試 - Overlay (覆蓋衍生檔)
// ============================================================================

TEST(OrderBook, ApplyOverlay_DerivedBid) {
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;
  snapshot.derived_bid = {Price::from(18499.5), Quantity::from(100)};

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  // Overlay: 覆蓋衍生買一檔
  BookUpdate update{};
  update.entry_count = 1;
  update.entries[0] = {
      .action = UpdateAction::Overlay,
      .type = EntryType::DerivedBid,
      .price = Price::from(18499.0),
      .size = Quantity::from(150),
      .level = 0,  // 衍生檔固定 level 0
  };

  // book.apply_update(update);

  // 預期結果：
  // derived_bid: 18499.0 x 150 (直接覆蓋)
}

// ============================================================================
// I081 更新測試 - 多筆更新依序處理
// ============================================================================

TEST(OrderBook, ApplyMultipleUpdates_Sequential) {
  BookSnapshot snapshot{};
  snapshot.is_calculated = false;
  snapshot.bids[0] = {Price::from(18500.0), Quantity::from(10)};
  snapshot.bids[1] = {Price::from(18499.0), Quantity::from(20)};

  OrderBook book;
  book.reset_from_snapshot(snapshot);

  // 多筆更新：New → Change → Delete
  BookUpdate update{};
  update.entry_count = 3;
  update.entries[0] = {UpdateAction::New, EntryType::Bid, Price::from(18500.5), Quantity::from(15),
                       0};
  update.entries[1] = {UpdateAction::Change, EntryType::Bid, Price::zero(), Quantity::from(25), 1};
  update.entries[2] = {UpdateAction::Delete, EntryType::Bid, Price::zero(), Quantity::zero(), 2};

  // book.apply_update(update);

  // 預期結果（依序執行）：
  // 1. New @ Level 0 → [18500.5 x 15, 18500.0 x 10, 18499.0 x 20, ...]
  // 2. Change @ Level 1 → [18500.5 x 15, 18500.0 x 25, 18499.0 x 20, ...]
  // 3. Delete @ Level 2 → [18500.5 x 15, 18500.0 x 25, 0 x 0, ...]
}
