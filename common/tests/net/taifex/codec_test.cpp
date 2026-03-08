#include "onyx/net/taifex/codec.hpp"

#include <gtest/gtest.h>

#include "onyx/core/type.hpp"

namespace {

using namespace onyx::core;
using namespace onyx::net::taifex;

}  // namespace

TEST(Codec, UnpackBCDPrice_TXF) {
  // 台指期價格：18500.50 (decimal_places = 2)
  // PACK BCD: 0x01 0x85 0x00 0x50 → 1850050
  uint8_t bcd[] = {0x01, 0x85, 0x00, 0x50};

  Price price = unpack_bcd_price(bcd, 2);

  // 預期：18500.50
  EXPECT_DOUBLE_EQ(price.to_double(), 18500.50);

  // 內部表示：18500.50 * 10000 = 185005000
  EXPECT_EQ(price.raw(), 185005000);
}

TEST(Codec, UnpackBCDTime) {
  // 測試時間: 13:30:05.123456
  uint8_t bcd[] = {0x13, 0x30, 0x05, 0x12, 0x34, 0x56};

  auto ns = unpack_bcd_time(bcd);

  auto expected = std::chrono::hours(13) + std::chrono::minutes(30) + std::chrono::seconds(5) +
                  std::chrono::milliseconds(123) + std::chrono::microseconds(456);

  EXPECT_EQ(ns, expected);
  // 預期：13*3600 + 30*60 + 5 = 48605 秒
  //       + 123 毫秒 + 456 微秒
  //       = 48605123456000 納秒
  EXPECT_EQ(ns.count(), 48605123456000LL);
}
