#include <benchmark/benchmark.h>

#include "tx/sync/spsc_queue.hpp"

namespace tx::sync::bench {

static void BM_SPSCQueue_PushPop(benchmark::State& state) {
  SPSCQueue<int, 1024> queue;

  for (auto _ : state) {
    benchmark::DoNotOptimize(queue.try_push(42));
    benchmark::DoNotOptimize(queue.try_pop());
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_SPSCQueue_PushPop);

}  // namespace tx::sync::bench
