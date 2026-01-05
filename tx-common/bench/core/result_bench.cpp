#include <benchmark/benchmark.h>

#include <random>
#include <string>

#include "tx/core/result.hpp"

// C++23 std::expected 僅在 benchmark 中啟用
#if __cplusplus >= 202302L && __has_include(<expected>)
#include <expected>
#define HAS_STD_EXPECTED 1
#else
#define HAS_STD_EXPECTED 0
#endif

namespace tx::core::bench {

// =============================================================================
// 測試數據結構（模擬真實交易數據）
// =============================================================================
struct TradeData {
  uint64_t order_id;
  double price;
  uint32_t quantity;
};

enum class ErrorCode : uint32_t {
  None = 0,
  InvalidPrice,
  InvalidQuantity,
  RiskLimitExceeded
};

// =============================================================================
// 1. C-Style: Error Code + Pointer Out-parameter (傳統做法)
// =============================================================================
ErrorCode c_style_ptr(uint32_t q, TradeData* out) {
  if (q == 0) [[unlikely]]
    return ErrorCode::InvalidQuantity;
  out->order_id = 12345;
  out->price = 100.5;
  out->quantity = q;
  return ErrorCode::None;
}

static void BM_C_Style_Pointer_Success(benchmark::State& state) {
  TradeData out;
  for (auto _ : state) {
    ErrorCode err = c_style_ptr(100, &out);
    if (err == ErrorCode::None) [[likely]] {
      benchmark::DoNotOptimize(out);
    }
  }
}
BENCHMARK(BM_C_Style_Pointer_Success);

static void BM_C_Style_Pointer_Failure(benchmark::State& state) {
  TradeData out;
  for (auto _ : state) {
    ErrorCode err = c_style_ptr(0, &out);  // 故意失敗
    if (err == ErrorCode::None) [[likely]] {
      benchmark::DoNotOptimize(out);
    } else {
      benchmark::DoNotOptimize(err);
    }
  }
}
BENCHMARK(BM_C_Style_Pointer_Failure);

// =============================================================================
// 2. C-Style: Combined Struct (手動封裝 Result)
// =============================================================================
struct CResult {
  TradeData data;
  ErrorCode err;
};

CResult c_style_struct(uint32_t q) {
  if (q == 0) [[unlikely]]
    return {{}, ErrorCode::InvalidQuantity};
  return {{12345, 100.5, q}, ErrorCode::None};
}

static void BM_C_Style_Struct_Success(benchmark::State& state) {
  for (auto _ : state) {
    CResult res = c_style_struct(100);
    if (res.err == ErrorCode::None) [[likely]] {
      benchmark::DoNotOptimize(res.data);
    }
  }
}
BENCHMARK(BM_C_Style_Struct_Success);

static void BM_C_Style_Struct_Failure(benchmark::State& state) {
  for (auto _ : state) {
    CResult res = c_style_struct(0);
    if (res.err == ErrorCode::None) [[likely]] {
      benchmark::DoNotOptimize(res.data);
    } else {
      benchmark::DoNotOptimize(res.err);
    }
  }
}
BENCHMARK(BM_C_Style_Struct_Failure);

// =============================================================================
// 3. tx::Result（您的實作）
// =============================================================================
tx::Result<TradeData, ErrorCode> tx_result_style(uint32_t q) {
  if (q == 0) [[unlikely]]
    return tx::Err(ErrorCode::InvalidQuantity);
  return tx::Ok(TradeData{12345, 100.5, q});
}

static void BM_Tx_Result_Success(benchmark::State& state) {
  for (auto _ : state) {
    auto res = tx_result_style(100);
    if (res.is_ok()) [[likely]] {
      benchmark::DoNotOptimize(*res);
    }
  }
}
BENCHMARK(BM_Tx_Result_Success);

static void BM_Tx_Result_Failure(benchmark::State& state) {
  for (auto _ : state) {
    auto res = tx_result_style(0);
    if (res.is_ok()) [[likely]] {
      benchmark::DoNotOptimize(*res);
    } else {
      benchmark::DoNotOptimize(res.error());
    }
  }
}
BENCHMARK(BM_Tx_Result_Failure);

// =============================================================================
// 4. std::expected (C++23)
// =============================================================================
#if HAS_STD_EXPECTED

std::expected<TradeData, ErrorCode> std_expected_style(uint32_t q) {
  if (q == 0) [[unlikely]]
    return std::unexpected(ErrorCode::InvalidQuantity);
  return TradeData{12345, 100.5, q};
}

static void BM_Std_Expected_Success(benchmark::State& state) {
  for (auto _ : state) {
    auto res = std_expected_style(100);
    if (res.has_value()) [[likely]] {
      benchmark::DoNotOptimize(*res);
    }
  }
}
BENCHMARK(BM_Std_Expected_Success);

static void BM_Std_Expected_Failure(benchmark::State& state) {
  for (auto _ : state) {
    auto res = std_expected_style(0);
    if (res.has_value()) [[likely]] {
      benchmark::DoNotOptimize(*res);
    } else {
      benchmark::DoNotOptimize(res.error());
    }
  }
}
BENCHMARK(BM_Std_Expected_Failure);

#endif

// =============================================================================
// 5. 鏈式調用測試（測試組合性開銷）
// =============================================================================

// C-Style 鏈式調用
static void BM_C_Style_Chaining(benchmark::State& state) {
  auto step1 = [](TradeData data, ErrorCode* err) -> TradeData {
    if (data.quantity > 10000) {
      *err = ErrorCode::InvalidQuantity;
      return {};
    }
    *err = ErrorCode::None;
    data.quantity *= 2;
    return data;
  };

  auto step2 = [](TradeData data, ErrorCode* err) -> TradeData {
    if (data.price > 1000000.0) {
      *err = ErrorCode::InvalidPrice;
      return {};
    }
    *err = ErrorCode::None;
    data.price *= 1.1;
    return data;
  };

  for (auto _ : state) {
    TradeData data{12345, 100.5, 100};
    ErrorCode err = ErrorCode::None;

    data = step1(data, &err);
    if (err != ErrorCode::None) {
      benchmark::DoNotOptimize(err);
      continue;
    }

    data = step2(data, &err);
    if (err != ErrorCode::None) {
      benchmark::DoNotOptimize(err);
      continue;
    }

    benchmark::DoNotOptimize(data);
  }
}
BENCHMARK(BM_C_Style_Chaining);

// tx::Result 鏈式調用
static void BM_Tx_Result_Chaining(benchmark::State& state) {
  auto step1 = [](TradeData data) -> tx::Result<TradeData, ErrorCode> {
    if (data.quantity > 10000) [[unlikely]]
      return tx::Err(ErrorCode::InvalidQuantity);
    data.quantity *= 2;
    return tx::Ok(data);
  };

  auto step2 = [](TradeData data) -> tx::Result<TradeData, ErrorCode> {
    if (data.price > 1000000.0) [[unlikely]]
      return tx::Err(ErrorCode::InvalidPrice);
    data.price *= 1.1;
    return tx::Ok(data);
  };

  for (auto _ : state) {
    auto result =
        tx::Result<TradeData, ErrorCode>(tx::Ok(TradeData{12345, 100.5, 100}))
            .and_then(step1)
            .and_then(step2);

    if (result.is_ok()) [[likely]] {
      benchmark::DoNotOptimize(*result);
    } else {
      benchmark::DoNotOptimize(result.error());
    }
  }
}
BENCHMARK(BM_Tx_Result_Chaining);

#if HAS_STD_EXPECTED

// std::expected 鏈式調用
static void BM_Std_Expected_Chaining(benchmark::State& state) {
  auto step1 = [](TradeData data) -> std::expected<TradeData, ErrorCode> {
    if (data.quantity > 10000) [[unlikely]]
      return std::unexpected(ErrorCode::InvalidQuantity);
    data.quantity *= 2;
    return data;
  };

  auto step2 = [](TradeData data) -> std::expected<TradeData, ErrorCode> {
    if (data.price > 1000000.0) [[unlikely]]
      return std::unexpected(ErrorCode::InvalidPrice);
    data.price *= 1.1;
    return data;
  };

  for (auto _ : state) {
    auto result =
        std::expected<TradeData, ErrorCode>(TradeData{12345, 100.5, 100})
            .and_then(step1)
            .and_then(step2);

    if (result.has_value()) [[likely]] {
      benchmark::DoNotOptimize(*result);
    } else {
      benchmark::DoNotOptimize(result.error());
    }
  }
}
BENCHMARK(BM_Std_Expected_Chaining);

#endif

// =============================================================================
// 6. 記憶體佈局比較
// =============================================================================
static void BM_SizeOf_Comparison(benchmark::State& state) {
  for (auto _ : state) {
    auto s1 = sizeof(CResult);
    auto s2 = sizeof(tx::Result<TradeData, ErrorCode>);
#if HAS_STD_EXPECTED
    auto s3 = sizeof(std::expected<TradeData, ErrorCode>);
    benchmark::DoNotOptimize(s3);
#endif
    benchmark::DoNotOptimize(s1);
    benchmark::DoNotOptimize(s2);
  }
}
BENCHMARK(BM_SizeOf_Comparison);

}  // namespace tx::core::bench
