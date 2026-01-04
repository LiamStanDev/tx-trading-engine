#include <benchmark/benchmark.h>

#include <random>
#include <string>

#include "tx/core/result.hpp"

namespace tx::core::bench {

// =============================================================================
// 測試用的錯誤類型：在交易引擎中，建議 Err 盡量輕量化
// =============================================================================
struct Error {
  int code;
  // 注意：包含 std::string 會讓 Result 變大，且構造 Err 時會觸發分配
  std::string message;
};

// =============================================================================
// 1. 構造與基礎操作 (應該在 0.2ns - 2ns 之間)
// =============================================================================

static void BM_Result_ConstructOk_Int(benchmark::State& state) {
  for (auto _ : state) {
    Result<int, int> r = Ok(42);
    benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Result_ConstructOk_Int);

static void BM_Result_ConstructErr_Int(benchmark::State& state) {
  for (auto _ : state) {
    Result<int, int> r = Err(404);
    benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Result_ConstructErr_Int);

// 測試 Result<void>：這對交易引擎的 Action 檢查非常重要
static void BM_Result_Void_Ok(benchmark::State& state) {
  for (auto _ : state) {
    Result<void, int> r = Ok<void>();
    benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Result_Void_Ok);

// =============================================================================
// 2. 新增：分支預測測試 - 測試 [[likely]] 的實際影響
// =============================================================================

// 測試成功路徑（90% Ok, 10% Err）- 模擬正常業務場景
static void BM_Result_Branch_MostlyOk(benchmark::State& state) {
  std::mt19937 rng(12345);  // 固定種子確保可重現
  std::uniform_int_distribution<int> dist(1, 10);

  for (auto _ : state) {
    // 90% 機率返回 Ok
    Result<int, int> r = (dist(rng) <= 9) ? Result<int, int>(Ok(42))
                                          : Result<int, int>(Err(404));

    if (r.is_ok()) {  // ← 這裡受 [[likely]] 影響
      int val = std::move(r).unwrap();
      benchmark::DoNotOptimize(val);
    } else {
      int err = std::move(r).error();
      benchmark::DoNotOptimize(err);
    }
  }
}
BENCHMARK(BM_Result_Branch_MostlyOk);

// 測試失敗路徑（10% Ok, 90% Err）- 異常場景
static void BM_Result_Branch_MostlyErr(benchmark::State& state) {
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(1, 10);

  for (auto _ : state) {
    // 10% 機率返回 Ok
    Result<int, int> r = (dist(rng) <= 1) ? Result<int, int>(Ok(42))
                                          : Result<int, int>(Err(404));

    if (r.is_ok()) {
      int val = std::move(r).unwrap();
      benchmark::DoNotOptimize(val);
    } else {
      int err = std::move(r).error();
      benchmark::DoNotOptimize(err);
    }
  }
}
BENCHMARK(BM_Result_Branch_MostlyErr);

// 測試 100% 成功路徑 - 最佳情況
static void BM_Result_Branch_AlwaysOk(benchmark::State& state) {
  for (auto _ : state) {
    Result<int, int> r = Ok(42);

    if (r.is_ok()) {  // ← [[likely]] 在這裡最有效
      int val = std::move(r).unwrap();
      benchmark::DoNotOptimize(val);
    }
  }
}
BENCHMARK(BM_Result_Branch_AlwaysOk);

// 測試 100% 失敗路徑
static void BM_Result_Branch_AlwaysErr(benchmark::State& state) {
  for (auto _ : state) {
    Result<int, int> r = Err(404);

    if (r.is_ok()) {
      int val = std::move(r).unwrap();
      benchmark::DoNotOptimize(val);
    } else {
      int err = std::move(r).error();
      benchmark::DoNotOptimize(err);
    }
  }
}
BENCHMARK(BM_Result_Branch_AlwaysErr);

// =============================================================================
// 3. 字串操作：區分 SSO 與 Heap Allocation
// =============================================================================

// SSO (Small String Optimization): 不應有 malloc
static void BM_Result_String_SSO_Move(benchmark::State& state) {
  for (auto _ : state) {
    // "short" 只有 5 bytes，觸發 SSO
    Result<std::string, int> r = Ok(std::string("short"));
    std::string s = std::move(r).unwrap();
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(BM_Result_String_SSO_Move);

// Heap: 必定有 malloc/free，這才是那 100ns+ 的來源
static void BM_Result_String_Heap_Move(benchmark::State& state) {
  std::string long_str(1024, 'a');
  for (auto _ : state) {
    Result<std::string, int> r = Ok(long_str);  // 這裡有一次 Copy
    std::string s = std::move(r).unwrap();      // 這裡 Move
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(BM_Result_String_Heap_Move);

// =============================================================================
// 4. 鏈式調用：模擬真實業務邏輯 (and_then / map)
// =============================================================================

static void BM_Result_Chaining_Logic(benchmark::State& state) {
  auto step1 = [](int x) -> Result<int, int> { return Ok(x + 1); };
  auto step2 = [](int x) -> Result<int, int> { return Ok(x * 2); };

  for (auto _ : state) {
    Result<int, int> r = Ok(10);
    auto res = std::move(r).and_then(step1).and_then(step2);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_Result_Chaining_Logic);

// 新增：真實鏈式調用（with branching）
static void BM_Result_Chaining_WithBranch(benchmark::State& state) {
  // 模擬真實場景：可能失敗的操作
  auto validate = [](int x) -> Result<int, int> {
    if (x < 0) return Err(-1);  // 驗證失敗
    return Ok(x);
  };
  auto process = [](int x) -> Result<int, int> {
    if (x > 1000) return Err(-2);  // 處理失敗
    return Ok(x * 2);
  };
  auto finalize = [](int x) -> Result<int, int> { return Ok(x + 100); };

  for (auto _ : state) {
    Result<int, int> r = Ok(10);
    auto res = std::move(r)
                   .and_then(validate)  // ← 這些分支受 [[likely]] 影響
                   .and_then(process)
                   .and_then(finalize);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_Result_Chaining_WithBranch);

// 新增：map 操作測試（全部成功）
static void BM_Result_Map_AllSuccess(benchmark::State& state) {
  auto add_one = [](int x) { return x + 1; };
  auto multiply = [](int x) { return x * 2; };

  for (auto _ : state) {
    Result<int, int> r = Ok(10);
    auto res = std::move(r)
                   .map(add_one)  // ← [[likely]] 路徑
                   .map(multiply);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_Result_Map_AllSuccess);

// 新增：map_err 測試
static void BM_Result_MapErr_MostlyOk(benchmark::State& state) {
  auto convert_error = [](int e) { return e * -1; };
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(1, 10);

  for (auto _ : state) {
    // 90% Ok, 10% Err
    Result<int, int> r = (dist(rng) <= 9) ? Result<int, int>(Ok(42))
                                          : Result<int, int>(Err(404));
    auto res = std::move(r).map_err(convert_error);  // ← [[unlikely]] 路徑
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_Result_MapErr_MostlyOk);

// 對比原始的 if-check 鏈式調用
static void BM_Raw_IfCheck_Logic(benchmark::State& state) {
  auto step1 = [](int x, int& out) -> bool {
    out = x + 1;
    return true;
  };
  auto step2 = [](int x, int& out) -> bool {
    out = x * 2;
    return true;
  };

  for (auto _ : state) {
    int val = 10;
    int out1, out2;
    if (step1(val, out1)) {
      if (step2(out1, out2)) {
        benchmark::DoNotOptimize(out2);
      }
    }
  }
}
BENCHMARK(BM_Raw_IfCheck_Logic);

// 新增：unwrap_or 測試
static void BM_Result_UnwrapOr_MostlyOk(benchmark::State& state) {
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(1, 10);

  for (auto _ : state) {
    Result<int, int> r = (dist(rng) <= 9) ? Result<int, int>(Ok(42))
                                          : Result<int, int>(Err(404));
    int val = std::move(r).unwrap_or(0);  // ← [[likely]] 走 Ok 路徑
    benchmark::DoNotOptimize(val);
  }
}
BENCHMARK(BM_Result_UnwrapOr_MostlyOk);

// 新增：unwrap_or_else 測試
static void BM_Result_UnwrapOrElse_MostlyOk(benchmark::State& state) {
  auto fallback = [](int e) { return e * -1; };
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(1, 10);

  for (auto _ : state) {
    Result<int, int> r = (dist(rng) <= 9) ? Result<int, int>(Ok(42))
                                          : Result<int, int>(Err(404));
    int val = std::move(r).unwrap_or_else(fallback);  // ← [[likely]] 走 Ok 路徑
    benchmark::DoNotOptimize(val);
  }
}
BENCHMARK(BM_Result_UnwrapOrElse_MostlyOk);

// =============================================================================
// 5. 記憶體佈局檢查 (編譯期常數，測試執行極快)
// =============================================================================

static void BM_Result_SizeOf_Exploration(benchmark::State& state) {
  struct LargeError {
    char data[128];
  };
  for (auto _ : state) {
    auto s1 = sizeof(Result<int, int>);
    auto s2 = sizeof(Result<int, LargeError>);
    auto s3 = sizeof(Result<void, int>);
    benchmark::DoNotOptimize(s1);
    benchmark::DoNotOptimize(s2);
    benchmark::DoNotOptimize(s3);
  }
}
BENCHMARK(BM_Result_SizeOf_Exploration);

// =============================================================================
// 6. 真實場景模擬 - 交易引擎典型使用
// =============================================================================

// 模擬訂單驗證流程
static void BM_Result_OrderValidation_Success(benchmark::State& state) {
  struct Order {
    int id;
    int quantity;
    double price;
  };

  auto validate_quantity = [](const Order& o) -> Result<Order, int> {
    if (o.quantity <= 0 || o.quantity > 10000) return Err(1);
    return Ok(o);
  };

  auto validate_price = [](const Order& o) -> Result<Order, int> {
    if (o.price <= 0.0 || o.price > 1000000.0) return Err(2);
    return Ok(o);
  };

  auto check_risk = [](const Order& o) -> Result<Order, int> {
    if (o.quantity * o.price > 5000000.0) return Err(3);
    return Ok(o);
  };

  for (auto _ : state) {
    Order order{12345, 100, 150.5};
    Result<Order, int> r = Ok(order);

    auto result = std::move(r)
                      .and_then(validate_quantity)  // ← 多個 [[likely]] 分支
                      .and_then(validate_price)
                      .and_then(check_risk);

    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_Result_OrderValidation_Success);

// 模擬網路操作：大多數成功，偶爾失敗
static void BM_Result_NetworkOp_Realistic(benchmark::State& state) {
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> dist(1, 100);

  auto send_packet = [&]() -> Result<size_t, int> {
    // 95% 成功率
    if (dist(rng) <= 95) return Ok(static_cast<size_t>(1024));
    return Err(-1);  // EAGAIN or similar
  };

  for (auto _ : state) {
    auto result = send_packet();

    if (result.is_ok()) {  // ← [[likely]] 應該幫助這裡
      size_t bytes = std::move(result).unwrap();
      benchmark::DoNotOptimize(bytes);
    } else {
      int err = std::move(result).error();
      benchmark::DoNotOptimize(err);
    }
  }
}
BENCHMARK(BM_Result_NetworkOp_Realistic);

// =============================================================================
// 7. 對比測試：with vs without Result
// =============================================================================

// 使用 Result 的版本
static void BM_WithResult_ErrorHandling(benchmark::State& state) {
  auto operation = [](int x) -> Result<int, int> {
    if (x < 0) return Err(-1);
    if (x > 1000) return Err(-2);
    return Ok(x * 2 + 10);
  };

  for (auto _ : state) {
    auto r1 = operation(50);
    auto r2 = std::move(r1).and_then(operation);
    auto r3 = std::move(r2).and_then(operation);
    benchmark::DoNotOptimize(r3);
  }
}
BENCHMARK(BM_WithResult_ErrorHandling);

// 使用 error code 的版本（對比）
static void BM_WithErrorCode_ErrorHandling(benchmark::State& state) {
  auto operation = [](int x, int* err) -> int {
    if (x < 0) {
      *err = -1;
      return 0;
    }
    if (x > 1000) {
      *err = -2;
      return 0;
    }
    *err = 0;
    return x * 2 + 10;
  };

  for (auto _ : state) {
    int err = 0;
    int v1 = operation(50, &err);
    if (err != 0) {
      benchmark::DoNotOptimize(err);
      continue;
    }

    int v2 = operation(v1, &err);
    if (err != 0) {
      benchmark::DoNotOptimize(err);
      continue;
    }

    int v3 = operation(v2, &err);
    benchmark::DoNotOptimize(v3);
  }
}
BENCHMARK(BM_WithErrorCode_ErrorHandling);

}  // namespace tx::core::bench
