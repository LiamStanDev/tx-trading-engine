#include <gtest/gtest.h>

#include <onyx/core/type.hpp>

using onyx::core::Price;

TEST(PriceTest, Construction) {
  // 從點數建立
  auto p1 = Price::from_double(18500.5);
  EXPECT_DOUBLE_EQ(p1.to_double(), 18500.5);
  EXPECT_EQ(p1.raw(), 185005000);

  // 從 ticks 建立
  auto p2 = Price::from_raw(185005000);
  EXPECT_EQ(p2.raw(), 185005000);
  EXPECT_DOUBLE_EQ(p2.to_double(), 18500.5);
}

TEST(PriceTest, Arithmetic) {
  auto p1 = Price::from_double(18500.0);
  auto p2 = Price::from_double(18505.0);

  // 價差
  auto diff = p2 - p1;
  EXPECT_DOUBLE_EQ(diff.to_double(), 5.0);

  // 縮放
  auto doubled = p1 * 2;
  EXPECT_DOUBLE_EQ(doubled.to_double(), 37000.0);
}

TEST(PriceTest, Comparison) {
  auto p1 = Price::from_double(18500.0);
  auto p2 = Price::from_double(18505.0);

  EXPECT_LT(p1, p2);
  EXPECT_LE(p1, p2);
  EXPECT_GT(p2, p1);
  EXPECT_GE(p2, p1);
  EXPECT_NE(p1, p2);

  auto p3 = Price::from_double(18500.0);
  EXPECT_EQ(p1, p3);
}

TEST(PriceTest, NoPrecisionLoss) {
  // 驗證不會有浮點數誤差
  auto base = Price::from_double(0.01);
  auto sum = Price::zero();

  for (int i = 0; i < 100; ++i) {
    sum = sum + base;
  }

  EXPECT_DOUBLE_EQ(sum.to_double(), 1.0);  // ✅ 精確！
}
