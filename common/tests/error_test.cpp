#include "onyx/error.hpp"

#include <gtest/gtest.h>

#include <cerrno>
#include <string>
#include <system_error>

namespace {
// Helpers to simulate operations
onyx::Result<int> trigger_fail(int ev) { return onyx::fail(ev, "Fail Triggered"); }

onyx::Result<int> trigger_bail(int ev) { return onyx::bail(ev, "Bail Triggered"); }

onyx::Result<int> trigger_wrap(int ev) { return onyx::wrap_errno(ev, "Wrap Triggered"); }

}  // namespace

class ErrorSystemTest : public ::testing::Test {};

// 1. Test Classification Logic (White-box testing of policy)
TEST_F(ErrorSystemTest, ClassifyErrnoPolicy) {
  using onyx::ErrorStrategy;

  // Group 1: Flow Control -> Bail
  EXPECT_EQ(onyx::classify_errno(EAGAIN), ErrorStrategy::Bail);
#if EWOULDBLOCK != EAGAIN
  EXPECT_EQ(onyx::classify_errno(EWOULDBLOCK), ErrorStrategy::Bail);
#endif
  EXPECT_EQ(onyx::classify_errno(EINTR), ErrorStrategy::Bail);

  // Group 2: Async Connection -> Bail
  EXPECT_EQ(onyx::classify_errno(EINPROGRESS), ErrorStrategy::Bail);
  EXPECT_EQ(onyx::classify_errno(EALREADY), ErrorStrategy::Bail);
  EXPECT_EQ(onyx::classify_errno(EISCONN), ErrorStrategy::Bail);

  // Group 3: Network Lifecycle -> Bail
  EXPECT_EQ(onyx::classify_errno(EPIPE), ErrorStrategy::Bail);
  EXPECT_EQ(onyx::classify_errno(ECONNRESET), ErrorStrategy::Bail);
  EXPECT_EQ(onyx::classify_errno(ETIMEDOUT), ErrorStrategy::Bail);

  // Group 6: Critical Errors -> Fail
  EXPECT_EQ(onyx::classify_errno(EBADF), ErrorStrategy::Fail);
  EXPECT_EQ(onyx::classify_errno(EFAULT), ErrorStrategy::Fail);
  EXPECT_EQ(onyx::classify_errno(EINVAL), ErrorStrategy::Fail);
  EXPECT_EQ(onyx::classify_errno(ENOMEM), ErrorStrategy::Fail);
  EXPECT_EQ(onyx::classify_errno(EADDRINUSE), ErrorStrategy::Fail);

  // Default -> Fail
  EXPECT_EQ(onyx::classify_errno(-1), ErrorStrategy::Fail);    // Unknown
  EXPECT_EQ(onyx::classify_errno(9999), ErrorStrategy::Fail);  // Unknown
}

// 2. Test Fail Strategy (Heavy)
TEST_F(ErrorSystemTest, FailCapturesStackTrace) {
  auto res = trigger_fail(EINVAL);
  ASSERT_FALSE(res.has_value());

  // 透過物件方法 context() 取回 Context
  const auto& ctx = res.error().context();
  EXPECT_EQ(ctx.ec.value(), EINVAL);
  EXPECT_GT(ctx.frame_count, 0);  // Should have frames
  EXPECT_STREQ(ctx.message, "Fail Triggered");
}

// 3. Test Bail Strategy (Light)
TEST_F(ErrorSystemTest, BailSkipsStackTrace) {
  auto res = trigger_bail(EAGAIN);
  ASSERT_FALSE(res.has_value());

  const auto& ctx = res.error().context();
  EXPECT_EQ(ctx.ec.value(), EAGAIN);
  EXPECT_EQ(ctx.frame_count, 0);  // Should NOT have frames
  EXPECT_STREQ(ctx.message, "Bail Triggered");
}

// 4. Test Wrap Errno (Auto Dispatch)
TEST_F(ErrorSystemTest, WrapErrnoDispatch) {
  // Case A: EAGAIN -> Bail
  {
    auto res = trigger_wrap(EAGAIN);
    ASSERT_FALSE(res.has_value());
    const auto& ctx = res.error().context();
    EXPECT_EQ(ctx.frame_count, 0);  // Bail behavior
  }

  // Case B: EBADF -> Fail
  {
    auto res = trigger_wrap(EBADF);
    ASSERT_FALSE(res.has_value());
    const auto& ctx = res.error().context();
    EXPECT_GT(ctx.frame_count, 0);  // Fail behavior
  }
}

// 5. Overloads Coverage
TEST_F(ErrorSystemTest, OverloadCoverage) {
  // fail(std::errc)
  {
    auto res = onyx::fail(std::errc::address_in_use, "errc fail");
    const auto& ctx = res.error().context();
    EXPECT_EQ(ctx.ec, std::make_error_code(std::errc::address_in_use));
    EXPECT_GT(ctx.frame_count, 0);
  }

  // fail(std::error_code)
  {
    auto ec = std::make_error_code(std::errc::io_error);
    auto res = onyx::fail(ec, "ec fail");
    const auto& ctx = res.error().context();
    EXPECT_EQ(ctx.ec, ec);
    EXPECT_GT(ctx.frame_count, 0);
  }

  // bail(std::errc)
  {
    auto res = onyx::bail(std::errc::operation_would_block, "errc bail");
    const auto& ctx = res.error().context();
    EXPECT_EQ(ctx.frame_count, 0);
  }

  // bail(std::error_code)
  {
    auto ec = std::make_error_code(std::errc::interrupted);
    auto res = onyx::bail(ec, "ec bail");
    const auto& ctx = res.error().context();
    EXPECT_EQ(ctx.frame_count, 0);
  }
}

// 6. Formatter Tests
TEST_F(ErrorSystemTest, FormatterOutput) {
  // Fail output (Deep)
  auto res_fail = onyx::fail(EBADF, "Bad File");
  // 利用新增的 fmt::formatter<onyx::ErrorHandle> 直接格式化 Handle
  std::string deep_out = fmt::format("{}", res_fail.error());
  EXPECT_NE(deep_out.find("[Error Diagnosis]"), std::string::npos);
  EXPECT_NE(deep_out.find("Stack Trace:"), std::string::npos);
  EXPECT_NE(deep_out.find("Bad File"), std::string::npos);

  // Bail output (Thin)
  auto res_bail = onyx::bail(EAGAIN, "Try Again");
  std::string thin_out = fmt::format("{}", res_bail.error());
  EXPECT_NE(thin_out.find("[Error Diagnosis]"), std::string::npos);
  EXPECT_EQ(thin_out.find("Stack Trace:"),
            std::string::npos);  // Should NOT appear
  EXPECT_NE(thin_out.find("Try Again"), std::string::npos);
}

// 7. Macro Tests
TEST_F(ErrorSystemTest, MacroBehavior) {
  // TRY success
  auto success_func = []() -> onyx::Result<int> { return 42; };
  auto try_test = [&]() -> onyx::Result<int> {
    int val = TRY(success_func());
    return val + 1;
  };
  EXPECT_EQ(*try_test(), 43);

  // TRY failure propagation
  auto fail_func = []() -> onyx::Result<int> { return onyx::bail(EPIPE); };
  auto try_fail_test = [&]() -> onyx::Result<int> {
    int val = TRY(fail_func());
    return val + 1;
  };
  auto res = try_fail_test();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().context().ec.value(), EPIPE);

  // CHECK failure propagation
  auto check_test = [&]() -> onyx::Result<> {
    CHECK(fail_func());
    return {};
  };
  auto check_res = check_test();
  ASSERT_FALSE(check_res.has_value());
  EXPECT_EQ(check_res.error().context().ec.value(), EPIPE);
}

// 8. RingBuffer Lifecycle & Expiration Protection
TEST_F(ErrorSystemTest, RegistryRingBufferLifecycle) {
  // 1. 驗證 RingBuffer 的核心價值：獨立保存多個錯誤不被覆蓋
  auto res1 = onyx::fail(EINVAL, "First Error");
  auto res2 = onyx::bail(EAGAIN, "Second Error");

  // 即使發生了 Second Error, First Error 的 Handle 依然有效且準確！
  const auto& ctx1 = res1.error().context();
  EXPECT_EQ(ctx1.ec.value(), EINVAL);
  EXPECT_STREQ(ctx1.message, "First Error");
  EXPECT_GT(ctx1.frame_count, 0);

  const auto& ctx2 = res2.error().context();
  EXPECT_EQ(ctx2.ec.value(), EAGAIN);
  EXPECT_STREQ(ctx2.message, "Second Error");
  EXPECT_EQ(ctx2.frame_count, 0);

  // 2. 驗證過期防禦機制 (Expiration Protection)
  // 假設 SIZE = 16，我們產生 16 個新錯誤，把 head_ 往前推，覆蓋掉最舊的槽位
  for (int i = 0; i < 16; ++i) {
    (void)onyx::bail(ENOENT, "Spam Error");
  }

  // 此時 res1 的 Handle 已經過期，context() 應該返回 EXPIRED_CONTEXT
  const auto& expired_ctx = res1.error().context();
  EXPECT_EQ(expired_ctx.ec, std::make_error_code(std::errc::state_not_recoverable));
  EXPECT_STREQ(expired_ctx.message, "<EXPIRED ERROR HANDLE>");
}
