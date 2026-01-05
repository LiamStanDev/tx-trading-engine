#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <tx/core/result.hpp>
#include <type_traits>
#include <vector>
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

// ===========================
// 9. 大物件移動語義測試
// ===========================

/// @brief 模擬大型物件（如訂單簿、市場數據等）
struct LargeObject {
  static constexpr size_t DATA_SIZE = 1024;
  std::array<uint64_t, DATA_SIZE> data;
  std::string metadata;
  int move_count = 0;
  int copy_count = 0;

  // 預設構造
  LargeObject() : data{}, metadata("default") {}

  // 值構造
  explicit LargeObject(int seed)
      : metadata("large_object_" + std::to_string(seed)) {
    for (size_t i = 0; i < DATA_SIZE; ++i) {
      data[i] = static_cast<uint64_t>(seed) + i;
    }
  }

  // Copy 構造（追蹤次數）
  LargeObject(const LargeObject& other)
      : data(other.data),
        metadata(other.metadata),
        move_count(other.move_count),
        copy_count(other.copy_count + 1) {}

  // Move 構造（追蹤次數）
  LargeObject(LargeObject&& other) noexcept
      : data(std::move(other.data)),
        metadata(std::move(other.metadata)),
        move_count(other.move_count + 1),
        copy_count(other.copy_count) {}

  // Copy 賦值
  LargeObject& operator=(const LargeObject& other) {
    if (this != &other) {
      data = other.data;
      metadata = other.metadata;
      move_count = other.move_count;
      copy_count = other.copy_count + 1;
    }
    return *this;
  }

  // Move 賦值
  LargeObject& operator=(LargeObject&& other) noexcept {
    if (this != &other) {
      data = std::move(other.data);
      metadata = std::move(other.metadata);
      move_count = other.move_count + 1;
      copy_count = other.copy_count;
    }
    return *this;
  }

  uint64_t checksum() const {
    uint64_t sum = 0;
    for (const auto& val : data) {
      sum += val;
    }
    return sum;
  }
};

// 測試：大物件構造不應產生不必要的複製
TEST(LargeObjectTest, ConstructionAvoidsCopy) {
  // 直接構造到 Result 中
  Result<LargeObject, int> r = Ok(LargeObject(42));

  EXPECT_TRUE(r.is_ok());
  // 應該只有移動，沒有複製
  EXPECT_EQ(r->copy_count, 0);
  EXPECT_GE(r->move_count, 1);  // 至少一次移動
}

// 測試：從 Result 移出大物件
TEST(LargeObjectTest, UnwrapMovesSingleTime) {
  Result<LargeObject, int> r = Ok(LargeObject(100));

  // 記錄移動前的狀態
  int initial_move_count = r->move_count;

  // 移出物件
  LargeObject obj = std::move(r).unwrap();

  // 應該只增加一次移動
  EXPECT_EQ(obj.move_count, initial_move_count + 1);
  EXPECT_EQ(obj.copy_count, 0);
  EXPECT_EQ(obj.checksum(),
            100 * LargeObject::DATA_SIZE +
                (LargeObject::DATA_SIZE * (LargeObject::DATA_SIZE - 1)) / 2);
}

// 測試：map 操作應保持移動語義
TEST(LargeObjectTest, MapPreservesMoveSemantics) {
  Result<LargeObject, int> r = Ok(LargeObject(50));

  // map 轉換：提取 checksum
  auto r2 = std::move(r).map([](LargeObject obj) {
    // 這裡 obj 是移動進來的
    EXPECT_EQ(obj.copy_count, 0);
    return obj.checksum();
  });

  EXPECT_TRUE(r2.is_ok());
  EXPECT_EQ(*r2,
            50 * LargeObject::DATA_SIZE +
                (LargeObject::DATA_SIZE * (LargeObject::DATA_SIZE - 1)) / 2);
}

// 測試：and_then 鏈式調用的移動效率
TEST(LargeObjectTest, AndThenChainMinimizesMoves) {
  auto validate = [](LargeObject obj) -> Result<LargeObject, int> {
    if (obj.data[0] < 1000) {
      return Ok(std::move(obj));  // 明確移動
    }
    return Err(-1);
  };

  auto process = [](LargeObject obj) -> Result<LargeObject, int> {
    obj.data[0] *= 2;  // 修改第一個元素
    return Ok(std::move(obj));
  };

  Result<LargeObject, int> r = Ok(LargeObject(10));

  auto result = std::move(r).and_then(validate).and_then(process);

  EXPECT_TRUE(result.is_ok());
  // 整個鏈式調用應該只有移動，沒有複製
  EXPECT_EQ(result->copy_count, 0);
  EXPECT_EQ(result->data[0], 20);  // 10 * 2
}

// 測試：錯誤路徑不應觸碰值
TEST(LargeObjectTest, ErrorPathDoesNotTouchValue) {
  auto failing_op = [](int x) -> Result<LargeObject, std::string> {
    if (x < 0) {
      return Err(std::string("negative value"));
    }
    return Ok(LargeObject(x));
  };

  auto result = failing_op(-1);

  EXPECT_TRUE(result.is_err());
  EXPECT_EQ(result.error(), "negative value");
  // 錯誤路徑下，LargeObject 根本不應該被構造
}

// 測試：unwrap_or 的移動語義
TEST(LargeObjectTest, UnwrapOrMovesCorrectly) {
  Result<LargeObject, int> r_ok = Ok(LargeObject(42));
  LargeObject default_obj(999);

  LargeObject result = std::move(r_ok).unwrap_or(std::move(default_obj));

  EXPECT_EQ(result.data[0], 42);
  EXPECT_EQ(result.copy_count, 0);  // 不應有複製

  // 測試錯誤情況
  Result<LargeObject, int> r_err = Err(404);
  LargeObject fallback(888);

  LargeObject result2 = std::move(r_err).unwrap_or(std::move(fallback));

  EXPECT_EQ(result2.data[0], 888);
  EXPECT_EQ(result2.copy_count, 0);  // 不應有複製
}

// 測試：unwrap_or_else 的惰性求值與移動
TEST(LargeObjectTest, UnwrapOrElseLazyMove) {
  int factory_called = 0;

  auto create_default = [&](int error_code) -> LargeObject {
    factory_called++;
    // 使用絕對值作為種子，避免負數問題
    return LargeObject(std::abs(error_code));
  };

  // Ok 情況：工廠不應被調用
  Result<LargeObject, int> r_ok = Ok(LargeObject(100));
  LargeObject obj1 = std::move(r_ok).unwrap_or_else(create_default);

  EXPECT_EQ(factory_called, 0);
  EXPECT_EQ(obj1.data[0], 100);
  EXPECT_EQ(obj1.copy_count, 0);

  // Err 情況：工廠應被調用一次
  Result<LargeObject, int> r_err = Err(404);
  LargeObject obj2 = std::move(r_err).unwrap_or_else(create_default);

  EXPECT_EQ(factory_called, 1);
  EXPECT_EQ(obj2.data[0], 404);  // abs(404) = 404
  EXPECT_EQ(obj2.copy_count, 0);
}

// 測試：複雜場景：大物件在多層 Result 中的傳遞
TEST(LargeObjectTest, ComplexNestedOperations) {
  // 模擬真實業務流程
  auto load_data = [](int id) -> Result<LargeObject, std::string> {
    if (id <= 0) return Err(std::string("invalid id"));
    return Ok(LargeObject(id));
  };

  auto validate_data = [](LargeObject obj) -> Result<LargeObject, std::string> {
    if (obj.data[0] > 10000) {
      return Err(std::string("data too large"));
    }
    return Ok(std::move(obj));
  };

  auto enrich_data = [](LargeObject obj) -> Result<LargeObject, std::string> {
    obj.metadata += "_enriched";
    return Ok(std::move(obj));
  };

  auto extract_checksum = [](LargeObject obj) -> Result<uint64_t, std::string> {
    return Ok(obj.checksum());
  };

  // 成功路徑
  auto result = load_data(123)
                    .and_then(validate_data)
                    .and_then(enrich_data)
                    .and_then(extract_checksum);

  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(*result,
            123 * LargeObject::DATA_SIZE +
                (LargeObject::DATA_SIZE * (LargeObject::DATA_SIZE - 1)) / 2);

  // 失敗路徑（驗證失敗）
  auto result2 = load_data(20000)
                     .and_then(validate_data)
                     .and_then(enrich_data)
                     .and_then(extract_checksum);

  EXPECT_TRUE(result2.is_err());
  EXPECT_EQ(result2.error(), "data too large");
}

// 測試：operator-> 在大物件上的使用
TEST(LargeObjectTest, ArrowOperatorOnLargeObject) {
  Result<LargeObject, int> r = Ok(LargeObject(777));

  // 透過 -> 訪問成員
  EXPECT_EQ(r->data[0], 777);
  EXPECT_EQ(r->metadata, "large_object_777");

  // 修改成員
  r->data[0] = 888;
  r->metadata = "modified";

  EXPECT_EQ(r->data[0], 888);
  EXPECT_EQ(r->metadata, "modified");

  // 呼叫成員函數
  uint64_t sum = r->checksum();
  EXPECT_GT(sum, 0);
}

// 測試：std::vector<LargeObject> 在 Result 中的行為
TEST(LargeObjectTest, VectorOfLargeObjectsInResult) {
  auto create_batch =
      [](size_t count) -> Result<std::vector<LargeObject>, std::string> {
    if (count == 0) return Err(std::string("count must be positive"));
    if (count > 100) return Err(std::string("count too large"));

    std::vector<LargeObject> batch;
    batch.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      batch.emplace_back(static_cast<int>(i));
    }
    return Ok(std::move(batch));
  };

  auto result = create_batch(5);

  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result->size(), 5);

  // 檢查每個物件
  for (size_t i = 0; i < result->size(); ++i) {
    EXPECT_EQ((*result)[i].data[0], i);
  }

  // 移出 vector
  std::vector<LargeObject> batch = std::move(result).unwrap();
  EXPECT_EQ(batch.size(), 5);
}

}  // namespace tx::core::test
