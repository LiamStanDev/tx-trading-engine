#include <gtest/gtest.h>

#include <tx/core/type.hpp>

namespace tx::core::test {

using tx::core::Price;

TEST(PriceTest, Construction) {
  // 從點數建立
  auto p1 = Price::from_points(18500.5);
  EXPECT_DOUBLE_EQ(p1.to_points(), 18500.5);
  EXPECT_EQ(p1.to_ticks(), 1850050);

  // 從 ticks 建立
  auto p2 = Price::from_ticks(1850050);
  EXPECT_EQ(p2.to_ticks(), 1850050);
  EXPECT_DOUBLE_EQ(p2.to_points(), 18500.5);
}

TEST(PriceTest, Arithmetic) {
  auto p1 = Price::from_points(18500.0);
  auto p2 = Price::from_points(18505.0);

  // 價差
  auto diff = p2 - p1;
  EXPECT_DOUBLE_EQ(diff.to_points(), 5.0);

  // 縮放
  auto doubled = p1 * 2;
  EXPECT_DOUBLE_EQ(doubled.to_points(), 37000.0);
}

TEST(PriceTest, Comparison) {
  auto p1 = Price::from_points(18500.0);
  auto p2 = Price::from_points(18505.0);

  EXPECT_LT(p1, p2);
  EXPECT_LE(p1, p2);
  EXPECT_GT(p2, p1);
  EXPECT_GE(p2, p1);
  EXPECT_NE(p1, p2);

  auto p3 = Price::from_points(18500.0);
  EXPECT_EQ(p1, p3);
}

TEST(PriceTest, NoPrecisionLoss) {
  // 驗證不會有浮點數誤差
  auto base = Price::from_points(0.01);
  auto sum = Price::zero();

  for (int i = 0; i < 100; ++i) {
    sum = sum + base;
  }

  EXPECT_DOUBLE_EQ(sum.to_points(), 1.0);  // ✅ 精確！
}

}  // namespace tx::core::test
