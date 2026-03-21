#include "onyx/error.hpp"

#include <gtest/gtest.h>

#include <cerrno>
#include <string>
#include <system_error>
#include <thread>

namespace {

onyx::Result<int> trigger_fail(int ev, std::string_view msg = "Fail Triggered") {
  return onyx::fail(ev, msg);
}

onyx::Result<int> trigger_bail(int ev) { return onyx::bail(ev); }

// 多層 TRY 傳播鏈
onyx::Result<int> layer_c() { return onyx::fail(EINVAL, "deep error"); }
onyx::Result<int> layer_b() { return TRY(layer_c()); }
onyx::Result<int> layer_a() { return TRY(layer_b()); }

}  // namespace

// ============================================================================
// ErrorToken 基本屬性
// ============================================================================

TEST(ErrorTokenTest, IsSingleByte) { EXPECT_EQ(sizeof(onyx::ErrorToken), 1u); }

// ============================================================================
// fail() — 完整捕捉策略
// ============================================================================

TEST(FailTest, CapturesErrorCode) {
  auto res = trigger_fail(EINVAL);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EINVAL);
}

TEST(FailTest, CapturesStackTrace) {
  auto res = trigger_fail(EINVAL);
  ASSERT_FALSE(res.has_value());
  EXPECT_GT(res.error().context().frame_count, 0);
}

TEST(FailTest, StoresMessage) {
  auto res = trigger_fail(EINVAL, "my error message");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().message, "my error message");
}

TEST(FailTest, EmptyMessageByDefault) {
  onyx::Result<int> res = onyx::fail(EBADF);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().message, "");
}

TEST(FailTest, CapturesSourceLocation) {
  onyx::Result<int> res = onyx::fail(EINVAL, "loc test");
  ASSERT_FALSE(res.has_value());
  const auto& loc = res.error().context().location;
  EXPECT_NE(std::string(loc.file_name()).find("error_test"), std::string::npos);
  EXPECT_GT(loc.line(), 0u);
}

// ============================================================================
// bail() — 輕量捕捉策略
// ============================================================================

TEST(BailTest, CapturesErrorCode) {
  auto res = trigger_bail(EAGAIN);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EAGAIN);
}

TEST(BailTest, SkipsStackTrace) {
  auto res = trigger_bail(EAGAIN);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().frame_count, 0);
}

TEST(BailTest, ClearsMessage) {
  // 先讓 fail 寫入訊息，再 bail，確認訊息被清空
  (void)trigger_fail(EINVAL, "previous message");
  auto res = trigger_bail(EAGAIN);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().message, "");
}

TEST(BailTest, CapturesSourceLocation) {
  onyx::Result<int> res = onyx::bail(EAGAIN);
  ASSERT_FALSE(res.has_value());
  const auto& loc = res.error().context().location;
  EXPECT_NE(std::string(loc.file_name()).find("error_test"), std::string::npos);
  EXPECT_GT(loc.line(), 0u);
}

// ============================================================================
// 多載覆蓋 (fail / bail × int / errc / error_code)
// ============================================================================

TEST(OverloadTest, FailWithInt) {
  onyx::Result<int> res = onyx::fail(EBADF, "int overload");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EBADF);
  EXPECT_GT(res.error().context().frame_count, 0);
}

TEST(OverloadTest, FailWithErrc) {
  onyx::Result<int> res = onyx::fail(std::errc::address_in_use, "errc overload");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec, std::make_error_code(std::errc::address_in_use));
  EXPECT_GT(res.error().context().frame_count, 0);
}

TEST(OverloadTest, FailWithErrorCode) {
  auto ec = std::make_error_code(std::errc::io_error);
  onyx::Result<int> res = onyx::fail(ec, "error_code overload");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec, ec);
  EXPECT_GT(res.error().context().frame_count, 0);
}

TEST(OverloadTest, BailWithInt) {
  onyx::Result<int> res = onyx::bail(EAGAIN);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EAGAIN);
  EXPECT_EQ(res.error().context().frame_count, 0);
}

TEST(OverloadTest, BailWithErrc) {
  onyx::Result<int> res = onyx::bail(std::errc::operation_would_block);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec, std::make_error_code(std::errc::operation_would_block));
  EXPECT_EQ(res.error().context().frame_count, 0);
}

TEST(OverloadTest, BailWithErrorCode) {
  auto ec = std::make_error_code(std::errc::interrupted);
  onyx::Result<int> res = onyx::bail(ec);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec, ec);
  EXPECT_EQ(res.error().context().frame_count, 0);
}

// ============================================================================
// ErrorToken 便利方法
// ============================================================================

TEST(ErrorTokenMethodTest, EcReturnsCorrectValue) {
  onyx::Result<int> res = onyx::fail(EPERM, "perm error");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().ec().value(), EPERM);
}

TEST(ErrorTokenMethodTest, MessageReturnsCopy) {
  onyx::Result<int> res = onyx::fail(EPERM, "token message");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().message(), "token message");
}

// ============================================================================
// 比較運算子
// ============================================================================

TEST(ComparisonTest, TokenEqualsErrorCode) {
  onyx::Result<int> res = onyx::fail(EINVAL, "cmp");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), onyx::make_error_code(EINVAL));
  EXPECT_NE(res.error(), onyx::make_error_code(EAGAIN));
}

TEST(ComparisonTest, TokenEqualsErrc) {
  onyx::Result<int> res = onyx::fail(std::errc::invalid_argument, "cmp errc");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), std::errc::invalid_argument);
  EXPECT_NE(res.error(), std::errc::operation_would_block);
}

TEST(ComparisonTest, ContextEqualsErrorCode) {
  onyx::Result<int> res = onyx::fail(EINVAL, "ctx cmp");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context(), onyx::make_error_code(EINVAL));
}

TEST(ComparisonTest, ContextEqualsErrc) {
  onyx::Result<int> res = onyx::fail(std::errc::invalid_argument, "ctx cmp errc");
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context(), std::errc::invalid_argument);
}

// ============================================================================
// 單槽覆寫語意 (Single Slot Semantics)
// ============================================================================

TEST(SingleSlotTest, LaterCallOverwritesPrevious) {
  (void)trigger_fail(EINVAL, "first");
  auto res = trigger_bail(EAGAIN);
  EXPECT_EQ(res.error().context().ec.value(), EAGAIN);
  EXPECT_EQ(res.error().context().frame_count, 0);
}

TEST(SingleSlotTest, RegistryGetReflectsLastCall) {
  (void)trigger_fail(EBADF, "registry test");
  EXPECT_EQ(onyx::ErrorRegistry::get().ec.value(), EBADF);

  (void)trigger_bail(EAGAIN);
  EXPECT_EQ(onyx::ErrorRegistry::get().ec.value(), EAGAIN);
}

// ============================================================================
// make_error_code
// ============================================================================

TEST(MakeErrorCodeTest, ConvertFromErrno) {
  auto ec = onyx::make_error_code(EINVAL);
  EXPECT_EQ(ec.value(), EINVAL);
  EXPECT_EQ(ec.category(), std::generic_category());
}

// ============================================================================
// TRY / CHECK 宏
// ============================================================================

TEST(MacroTest, TrySuccessPropagatesValue) {
  auto success = []() -> onyx::Result<int> { return 42; };
  auto caller = [&]() -> onyx::Result<int> {
    int v = TRY(success());
    return v + 1;
  };
  auto res = caller();
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(*res, 43);
}

TEST(MacroTest, TryFailurePropagatesError) {
  auto fail_fn = []() -> onyx::Result<int> { return onyx::bail(EPIPE); };
  auto caller = [&]() -> onyx::Result<int> {
    int v = TRY(fail_fn());
    return v;
  };
  auto res = caller();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EPIPE);
}

TEST(MacroTest, CheckFailurePropagatesError) {
  auto fail_fn = []() -> onyx::Result<int> { return onyx::bail(ECONNRESET); };
  auto caller = [&]() -> onyx::Result<> {
    CHECK(fail_fn());
    return {};
  };
  auto res = caller();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), ECONNRESET);
}

TEST(MacroTest, TryPropagatesThroughMultipleLayers) {
  // layer_a → layer_b → layer_c (fail EINVAL)
  // 每層只是 TRY 轉傳，最終錯誤應完整保留
  auto res = layer_a();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EINVAL);
  EXPECT_EQ(res.error().context().message, "deep error");
}

// ============================================================================
// 格式化輸出
// ============================================================================

TEST(FormatterTest, FailIncludesStackTrace) {
  auto res = trigger_fail(EBADF, "Bad File");
  std::string out = fmt::format("{}", res.error());
  EXPECT_NE(out.find("[Error Diagnosis]"), std::string::npos);
  EXPECT_NE(out.find("Stack Trace:"), std::string::npos);
  EXPECT_NE(out.find("Bad File"), std::string::npos);
}

TEST(FormatterTest, BailExcludesStackTrace) {
  auto res = trigger_bail(EAGAIN);
  std::string out = fmt::format("{}", res.error());
  EXPECT_NE(out.find("[Error Diagnosis]"), std::string::npos);
  EXPECT_EQ(out.find("Stack Trace:"), std::string::npos);
  // EAGAIN 的 strerror 描述應出現在輸出中
  EXPECT_NE(out.find("Resource temporarily unavailable"), std::string::npos);
}

TEST(FormatterTest, ContextFormatterDirectly) {
  auto res = trigger_fail(ENOMEM, "OOM");
  std::string out = fmt::format("{}", res.error().context());
  EXPECT_NE(out.find("[Error Diagnosis]"), std::string::npos);
  EXPECT_NE(out.find("OOM"), std::string::npos);
}

// ============================================================================
// 空訊息邊界條件
// ============================================================================

TEST(EdgeCaseTest, BailAlwaysHasEmptyMessage) {
  auto res = trigger_bail(EAGAIN);
  EXPECT_EQ(res.error().message(), "");
  EXPECT_EQ(res.error().context().message, "");
}

TEST(EdgeCaseTest, FailWithExplicitEmptyStringView) {
  onyx::Result<int> res = onyx::fail(EBADF, std::string_view{});
  EXPECT_EQ(res.error().message(), "");
  EXPECT_EQ(res.error().context().message, "");
}

// ============================================================================
// 執行緒隔離
// ============================================================================

TEST(ThreadTest, EachThreadHasIndependentSlot) {
  // 主執行緒寫入 EINVAL
  (void)trigger_fail(EINVAL, "main thread error");
  int ec_main = onyx::ErrorRegistry::get().ec.value();

  int ec_thread = 0;
  std::thread t([&] {
    // 子執行緒寫入 EAGAIN，不應影響主執行緒的 slot
    (void)trigger_fail(EAGAIN, "thread error");
    ec_thread = onyx::ErrorRegistry::get().ec.value();
  });
  t.join();

  // 主執行緒 slot 未被子執行緒覆蓋
  EXPECT_EQ(onyx::ErrorRegistry::get().ec.value(), ec_main);
  EXPECT_EQ(ec_main, EINVAL);
  EXPECT_EQ(ec_thread, EAGAIN);
}
