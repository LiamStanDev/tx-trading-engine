#include <benchmark/benchmark.h>

#include <string>

#include "tx/protocols/fix/parser.hpp"

namespace tx::protocols::fix::bench {

using namespace tx::protocols::fix;

static const std::string FIX_MSG =
    "8=FIX.4.2\x01"
    "9=40\x01"
    "35=D\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=1\x01"
    "10=150\x01";

static void BM_Parser_ValidMessage(benchmark::State& state) {
  Parser parser;
  for (auto _ : state) {
    auto result = parser.parse(FIX_MSG);

    benchmark::DoNotOptimize(result);

    if (result.is_err()) {
      state.SkipWithError("Parse failed!");
      break;
    }
  }

  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(FIX_MSG.size()));

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Parser_ValidMessage);

static void BM_Parser_LargeMsg(benchmark::State& state) {
  // 構造一個包含很多自定義欄位的超長訊息
  std::string large_msg = "8=FIX.4.4\0019=1000\00135=D\001";
  for (int i = 0; i < 100; ++i) {
    large_msg +=
        std::to_string(1000 + i) + "=Value" + std::to_string(i) + "\001";
  }
  large_msg += "10=000\001";

  for (auto _ : state) {
    benchmark::DoNotOptimize(Parser::parse(large_msg));
  }
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(FIX_MSG.size()));

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Parser_LargeMsg);

}  // namespace tx::protocols::fix::bench
