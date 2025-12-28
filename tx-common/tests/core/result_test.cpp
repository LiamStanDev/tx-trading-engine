#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <tx/core/result.hpp>
#include <type_traits>
namespace tx::core::test {
using tx::core::Err;
using tx::core::Ok;
using tx::core::Result;
// ===========================
// 測試用的錯誤類型
// ===========================
struct TestError {
  int code;
  std::string message;
  TestError(int c, std::string msg) : code(c), message(std::move(msg)) {}
  bool operator==(const TestError& other) const {
    return code == other.code && message == other.message;
  }
};
// ===========================
// 測試用的 Move-Only 類型
// ===========================
class MoveOnly {
 private:
  std::unique_ptr<int> data_;

 public:
  explicit MoveOnly(int value) : data_(std::make_unique<int>(value)) {}
  // Delete copy
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
  // Allow move
  MoveOnly(MoveOnly&&) noexcept = default;
  MoveOnly& operator=(MoveOnly&&) noexcept = default;
  int value() const { return *data_; }
};
// ===========================
// 1. 基礎功能測試
// ===========================
TEST(ResultTest, ConstructionOk) {
  Result<int, TestError> r = Ok(42);
  EXPECT_TRUE(r.is_ok());
  EXPECT_FALSE(r.is_err());
  EXPECT_TRUE(static_cast<bool>(r));
}
TEST(ResultTest, ConstructionErr) {
  Result<int, TestError> r = Err(TestError{404, "Not Found"});
  EXPECT_FALSE(r.is_ok());
  EXPECT_TRUE(r.is_err());
  EXPECT_FALSE(static_cast<bool>(r));
}
TEST(ResultTest, TypeAliases) {
  using R = Result<int, TestError>;
  static_assert(std::is_same_v<R::value_type, int>);
  static_assert(std::is_same_v<R::error_type, TestError>);
}
// ===========================
// 2. 值訪問測試
// ===========================
TEST(ResultTest, OperatorStarLvalue) {
  Result<int, TestError> r = Ok(42);
  EXPECT_EQ(*r, 42);
  // 可以修改
  *r = 100;
  EXPECT_EQ(*r, 100);
}
TEST(ResultTest, OperatorStarConstLvalue) {
  const Result<int, TestError> r = Ok(42);
  EXPECT_EQ(*r, 42);
  // 以下應該編譯失敗（const 不可修改）
  // *r = 100;
}
TEST(ResultTest, OperatorStarRvalue) {
  Result<std::string, TestError> r = Ok(std::string("hello"));
  // 移動出值
  std::string s = *std::move(r);
  EXPECT_EQ(s, "hello");
  // r 現在處於 moved-from 狀態（不應再使用）
}
TEST(ResultTest, OperatorArrow) {
  struct Point {
    int x, y;
  };
  Result<Point, TestError> r = Ok(Point{3, 4});
  EXPECT_EQ(r->x, 3);
  EXPECT_EQ(r->y, 4);
  r->x = 10;
  EXPECT_EQ(r->x, 10);
}
TEST(ResultTest, ValueMethod) {
  Result<int, TestError> r = Ok(42);
  // value() 和 operator* 應該等價
  EXPECT_EQ(r.value(), *r);
  EXPECT_EQ(r.value(), 42);
}
TEST(ResultTest, ErrorAccess) {
  Result<int, TestError> r = Err(TestError{500, "Internal Error"});
  EXPECT_EQ(r.error().code, 500);
  EXPECT_EQ(r.error().message, "Internal Error");
  // 可以修改
  r.error().code = 503;
  EXPECT_EQ(r.error().code, 503);
}
TEST(ResultTest, ErrorAccessRvalue) {
  Result<int, TestError> r = Err(TestError{404, "Not Found"});
  // 移動出錯誤
  TestError err = std::move(r).error();
  EXPECT_EQ(err.code, 404);
  EXPECT_EQ(err.message, "Not Found");
}
// ===========================
// 3. 消耗性方法測試
// ===========================
TEST(ResultTest, Unwrap) {
  Result<int, TestError> r = Ok(42);
  int value = std::move(r).unwrap();
  EXPECT_EQ(value, 42);
}
// Note: unwrap() on Err 會在 Debug 模式觸發 assert
// 這個測試在 Release 模式會是 UB，不建議測試
TEST(ResultTest, UnwrapOr) {
  Result<int, TestError> r1 = Ok(42);
  EXPECT_EQ(std::move(r1).unwrap_or(0), 42);
  Result<int, TestError> r2 = Err(TestError{404, "Error"});
  EXPECT_EQ(std::move(r2).unwrap_or(0), 0);
}
TEST(ResultTest, UnwrapOrElse) {
  Result<int, TestError> r1 = Ok(42);
  int value1 =
      std::move(r1).unwrap_or_else([](TestError err) { return -err.code; });
  EXPECT_EQ(value1, 42);
  Result<int, TestError> r2 = Err(TestError{404, "Error"});
  int value2 =
      std::move(r2).unwrap_or_else([](TestError err) { return -err.code; });
  EXPECT_EQ(value2, -404);
}
TEST(ResultTest, UnwrapOrElseLazyEvaluation) {
  int call_count = 0;
  Result<int, TestError> r1 = Ok(42);
  [[maybe_unused]] int v1 = std::move(r1).unwrap_or_else([&](TestError) {
    call_count++;
    return 0;
  });
  // Ok 時不應調用 lambda
  EXPECT_EQ(call_count, 0);
  Result<int, TestError> r2 = Err(TestError{500, "Error"});
  [[maybe_unused]] int v2 = std::move(r2).unwrap_or_else([&](TestError) {
    call_count++;
    return 0;
  });
  // Err 時應調用 lambda
  EXPECT_EQ(call_count, 1);
}
// ===========================
// 4. 函數式操作測試
// ===========================
TEST(ResultTest, MapOk) {
  Result<int, TestError> r = Ok(42);
  auto r2 = std::move(r).map([](int x) { return x * 2; });
  EXPECT_TRUE(r2.is_ok());
  EXPECT_EQ(*r2, 84);
}
TEST(ResultTest, MapErr) {
  Result<int, TestError> r = Err(TestError{404, "Not Found"});
  auto r2 = std::move(r).map([](int x) { return x * 2; });
  EXPECT_TRUE(r2.is_err());
  EXPECT_EQ(r2.error().code, 404);
}
TEST(ResultTest, MapTypeConversion) {
  Result<int, TestError> r = Ok(42);
  // int -> std::string
  auto r2 = std::move(r).map([](int x) { return std::to_string(x); });
  static_assert(std::is_same_v<decltype(r2), Result<std::string, TestError>>);
  EXPECT_EQ(*r2, "42");
}
TEST(ResultTest, AndThenOk) {
  Result<int, TestError> r = Ok(42);
  auto r2 = std::move(r).and_then([](int x) -> Result<std::string, TestError> {
    if (x > 0) {
      return Ok(std::to_string(x));
    }
    return Err(TestError{400, "Negative value"});
  });
  EXPECT_TRUE(r2.is_ok());
  EXPECT_EQ(*r2, "42");
}
TEST(ResultTest, AndThenErr) {
  Result<int, TestError> r = Err(TestError{500, "Server Error"});
  auto r2 = std::move(r).and_then([](int x) -> Result<std::string, TestError> {
    return Ok(std::to_string(x));
  });
  EXPECT_TRUE(r2.is_err());
  EXPECT_EQ(r2.error().code, 500);
}
TEST(ResultTest, AndThenChaining) {
  Result<int, TestError> r = Ok(10);
  auto final_result =
      std::move(r)
          .and_then([](int x) -> Result<int, TestError> {
            if (x > 0) return Ok(x * 2);
            return Err(TestError{400, "Negative"});
          })
          .and_then([](int x) -> Result<std::string, TestError> {
            return Ok(std::to_string(x));
          });
  EXPECT_TRUE(final_result.is_ok());
  EXPECT_EQ(*final_result, "20");
}
TEST(ResultTest, MapAndThenCombined) {
  Result<int, TestError> r = Ok(5);
  auto result =
      std::move(r)
          .map([](int x) { return x * 2; })  // 5 -> 10
          .and_then([](int x) -> Result<int, TestError> {
            if (x < 20) return Ok(x + 10);  // 10 -> 20
            return Err(TestError{400, "Too large"});
          })
          .map([](int x) { return std::to_string(x); });  // 20 -> "20"
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(*result, "20");
}
// ===========================
// 5. void 特化測試
// ===========================
TEST(ResultVoidTest, ConstructionOk) {
  Result<void, TestError> r = Ok<void>{};
  EXPECT_TRUE(r.is_ok());
  EXPECT_FALSE(r.is_err());
}
TEST(ResultVoidTest, ConstructionErr) {
  Result<void, TestError> r = Err(TestError{404, "Not Found"});
  EXPECT_FALSE(r.is_ok());
  EXPECT_TRUE(r.is_err());
}
TEST(ResultVoidTest, ErrorAccess) {
  Result<void, TestError> r = Err(TestError{500, "Error"});
  EXPECT_EQ(r.error().code, 500);
  EXPECT_EQ(r.error().message, "Error");
}
TEST(ResultVoidTest, OperatorBool) {
  Result<void, TestError> r1 = Ok<void>{};
  EXPECT_TRUE(static_cast<bool>(r1));
  Result<void, TestError> r2 = Err(TestError{404, "Error"});
  EXPECT_FALSE(static_cast<bool>(r2));
}

// ===========================
// 6. 移動語義測試
// ===========================
TEST(ResultTest, MoveOnlyType) {
  Result<MoveOnly, TestError> r = Ok(MoveOnly(42));
  EXPECT_TRUE(r.is_ok());
  EXPECT_EQ(r->value(), 42);
  // 移動出值
  MoveOnly obj = std::move(r).unwrap();
  EXPECT_EQ(obj.value(), 42);
}
TEST(ResultTest, MoveOnlyError) {
  struct MoveOnlyError {
    std::unique_ptr<std::string> msg;

    explicit MoveOnlyError(std::string s)
        : msg(std::make_unique<std::string>(std::move(s))) {}

    MoveOnlyError(MoveOnlyError&&) = default;
    MoveOnlyError& operator=(MoveOnlyError&&) = default;
  };
  Result<int, MoveOnlyError> r = Err(MoveOnlyError("error"));
  EXPECT_TRUE(r.is_err());
  MoveOnlyError err = std::move(r).error();
  EXPECT_EQ(*err.msg, "error");
}
TEST(ResultTest, RvalueCorrectness) {
  Result<std::string, TestError> r = Ok(std::string("hello"));
  // 這應該移動，不是拷貝
  std::string s = *std::move(r);
  EXPECT_EQ(s, "hello");
  // r.value() 現在是空字符串（moved-from）
  // 不應再訪問 r
}
// ===========================
// 7. noexcept 規範測試
// ===========================
TEST(ResultTest, NoexceptSpecifications) {
  using R = Result<int, TestError>;
  // 查詢方法應該是 noexcept
  static_assert(noexcept(std::declval<R>().is_ok()));
  static_assert(noexcept(std::declval<R>().is_err()));
  static_assert(noexcept(static_cast<bool>(std::declval<R>())));
  // 訪問方法應該是 noexcept
  static_assert(noexcept(*std::declval<R&>()));
  static_assert(noexcept(std::declval<R&>().value()));
  static_assert(noexcept(std::declval<R&>().error()));
  // unwrap 應該是 noexcept
  static_assert(noexcept(std::declval<R>().unwrap()));
}
TEST(ResultTest, ConditionalNoexcept) {
  struct NoThrowMove {
    NoThrowMove() = default;
    NoThrowMove(NoThrowMove&&) noexcept = default;
  };
  struct MayThrowMove {
    MayThrowMove() = default;
    MayThrowMove(MayThrowMove&&) {}
  };
  // NoThrowMove 的構造應該是 noexcept
  static_assert(noexcept(Result<NoThrowMove, TestError>(Ok(NoThrowMove{}))));
  // MayThrowMove 的構造不是 noexcept
  static_assert(!noexcept(Result<MayThrowMove, TestError>(Ok(MayThrowMove{}))));
}
// ===========================
// 8. Concept 約束驗證（編譯期測試）
// ===========================
// 這些測試通過編譯來驗證
TEST(ResultTest, ConceptConstraints) {
  // 可移動類型應該編譯通過
  Result<int, std::string> r1 = Ok(42);
  Result<std::string, int> r2 = Ok(std::string("hello"));
  Result<std::unique_ptr<int>, std::string> r3 = Ok(std::make_unique<int>(42));
  // 以下應該編譯失敗（取消註解測試）：

  // 不可移動類型
  // struct NonMovable {
  //   NonMovable(const NonMovable&) = delete;
  //   NonMovable(NonMovable&&) = delete;
  // };
  // Result<NonMovable, int> r4 = Ok(NonMovable{});
  // 不可析構類型
  // struct NonDestructible {
  //   ~NonDestructible() = delete;
  // };
  // Result<NonDestructible, int> r5 = Ok(NonDestructible{});
  SUCCEED();  // 如果編譯通過，測試成功
}
TEST(ResultTest, MapConceptConstraints) {
  Result<int, std::string> r = Ok(42);
  // 正確的 map
  auto r1 = std::move(r).map([](int x) { return x * 2; });
  // 以下應該編譯失敗（取消註解測試）：
  // 參數類型不匹配
  // auto r2 = std::move(r).map([](std::string s) { return s.size(); });
  // 返回不可移動類型
  // auto r3 = std::move(r).map([](int x) -> std::atomic<int> {
  //   return std::atomic<int>(x);
  // });
  SUCCEED();
}
TEST(ResultTest, AndThenConceptConstraints) {
  Result<int, std::string> r = Ok(42);
  // 正確的 and_then
  auto r1 = std::move(r).and_then(
      [](int x) -> Result<double, std::string> { return Ok(x * 2.0); });
  // 以下應該編譯失敗（取消註解測試）：
  // 返回不是 Result
  // auto r2 = std::move(r).and_then([](int x) { return x * 2; });
  // error_type 不匹配
  // auto r3 = std::move(r).and_then([](int x) -> Result<int, int> {
  //   return Ok(x);
  // });
  SUCCEED();
}
// ===========================
// 9. 實際使用場景測試
// ===========================
// 模擬解析整數
Result<int, std::string> parse_int(const std::string& s) {
  try {
    return Ok(std::stoi(s));
  } catch (...) {
    return Err(std::string("Invalid integer: ") + s);
  }
}
// 模擬除法
Result<double, std::string> divide(int a, int b) {
  if (b == 0) {
    return Err(std::string("Division by zero"));
  }
  return Ok(static_cast<double>(a) / b);
}
TEST(ResultTest, RealWorldScenario) {
  auto result = parse_int("42")
                    .and_then([](int x) { return divide(x, 2); })
                    .map([](double x) { return static_cast<int>(x); });
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(*result, 21);
}
TEST(ResultTest, RealWorldScenarioError) {
  auto result = parse_int("invalid")
                    .and_then([](int x) { return divide(x, 2); })
                    .map([](double x) { return static_cast<int>(x); });
  EXPECT_TRUE(result.is_err());
  EXPECT_EQ(result.error(), "Invalid integer: invalid");
}
TEST(ResultTest, RealWorldScenarioDivisionByZero) {
  auto result = parse_int("42")
                    .and_then([](int x) { return divide(x, 0); })
                    .map([](double x) { return static_cast<int>(x); });
  EXPECT_TRUE(result.is_err());
  EXPECT_EQ(result.error(), "Division by zero");
}

// ===========================
// 10. 性能測試（基準測試）
// ===========================
TEST(ResultTest, ZeroOverheadAbstraction) {
  // 驗證 Result 的大小接近底層 variant
  using R = Result<int, std::string>;
  using V = std::variant<int, std::string>;
  // Result 應該和 variant 大小相同（沒有額外開銷）
  EXPECT_EQ(sizeof(R), sizeof(V));
}
TEST(ResultTest, NoHeapAllocation) {
  // Result<int, int> 應該完全在棧上，不涉及堆分配
  // 驗證 trivially copyable（如果 T 和 E 都是）
  static_assert(std::is_trivially_destructible_v<Result<int, int>>);
}

// 測試 Result<T, E>::map_err
TEST(MapErrTest, TransformError) {
  enum class LowError { A, B };
  enum class HighError { X, Y };

  auto result = Result<int, LowError>(Err(LowError::A));
  auto transformed = std::move(result).map_err([](LowError e) {
    return e == LowError::A ? HighError::X : HighError::Y;
  });

  ASSERT_TRUE(transformed.is_err());
  EXPECT_EQ(transformed.error(), HighError::X);
}
// 測試 Result<void, E>::map_err
TEST(MapErrTest, VoidResultMapErr) {
  enum class ErrorA { Fail };
  enum class ErrorB { Bad };

  auto result = Result<void, ErrorA>(Err(ErrorA::Fail));
  auto transformed =
      std::move(result).map_err([](ErrorA) { return ErrorB::Bad; });

  ASSERT_TRUE(transformed.is_err());
  EXPECT_EQ(transformed.error(), ErrorB::Bad);
}

}  // namespace tx::core::test
