#include <benchmark/benchmark.h>

#include <string>
#include <tx/core/result.hpp>

namespace tx::core::benchmark {

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

static void BM_Result_ConstructOk_Int(::benchmark::State& state) {
  for (auto _ : state) {
    Result<int, int> r = Ok(42);
    ::benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Result_ConstructOk_Int);

static void BM_Result_ConstructErr_Int(::benchmark::State& state) {
  for (auto _ : state) {
    Result<int, int> r = Err(404);
    ::benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Result_ConstructErr_Int);

// 測試 Result<void>：這對交易引擎的 Action 檢查非常重要
static void BM_Result_Void_Ok(::benchmark::State& state) {
  for (auto _ : state) {
    Result<void, int> r = Ok<void>();
    ::benchmark::DoNotOptimize(r);
  }
}
BENCHMARK(BM_Result_Void_Ok);

// =============================================================================
// 2. 字串操作：區分 SSO 與 Heap Allocation
// =============================================================================

// SSO (Small String Optimization): 不應有 malloc
static void BM_Result_String_SSO_Move(::benchmark::State& state) {
  for (auto _ : state) {
    // "short" 只有 5 bytes，觸發 SSO
    Result<std::string, int> r = Ok(std::string("short"));
    std::string s = std::move(r).unwrap();
    ::benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(BM_Result_String_SSO_Move);

// Heap: 必定有 malloc/free，這才是那 100ns+ 的來源
static void BM_Result_String_Heap_Move(::benchmark::State& state) {
  std::string long_str(1024, 'a');
  for (auto _ : state) {
    Result<std::string, int> r = Ok(long_str);  // 這裡有一次 Copy
    std::string s = std::move(r).unwrap();      // 這裡 Move
    ::benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(BM_Result_String_Heap_Move);

// =============================================================================
// 3. 鏈式調用：模擬真實業務邏輯 (and_then / map)
// =============================================================================

static void BM_Result_Chaining_Logic(::benchmark::State& state) {
  auto step1 = [](int x) -> Result<int, int> { return Ok(x + 1); };
  auto step2 = [](int x) -> Result<int, int> { return Ok(x * 2); };

  for (auto _ : state) {
    Result<int, int> r = Ok(10);
    auto res = std::move(r).and_then(step1).and_then(step2);
    ::benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_Result_Chaining_Logic);

// 對比原始的 if-check 鏈式調用
static void BM_Raw_IfCheck_Logic(::benchmark::State& state) {
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
        ::benchmark::DoNotOptimize(out2);
      }
    }
  }
}
BENCHMARK(BM_Raw_IfCheck_Logic);

// =============================================================================
// 4. 記憶體佈局檢查 (編譯期常數，測試執行極快)
// =============================================================================

static void BM_Result_SizeOf_Exploration(::benchmark::State& state) {
  struct LargeError {
    char data[128];
  };
  for (auto _ : state) {
    auto s1 = sizeof(Result<int, int>);
    auto s2 = sizeof(Result<int, LargeError>);
    auto s3 = sizeof(Result<void, int>);
    ::benchmark::DoNotOptimize(s1);
    ::benchmark::DoNotOptimize(s2);
    ::benchmark::DoNotOptimize(s3);
  }
}
BENCHMARK(BM_Result_SizeOf_Exploration);

}  // namespace tx::core::benchmark

// Google Benchmark 啟動點
BENCHMARK_MAIN();
