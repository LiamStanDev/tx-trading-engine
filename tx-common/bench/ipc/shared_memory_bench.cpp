#include <benchmark/benchmark.h>
#include <sys/mman.h>

#include "tx/ipc/shared_memory.hpp"

namespace tx::ipc::bench {

// ============================================================================
// 工具
// ============================================================================
static void cleanup_shm(std::string_view name, bool is_huge) {
  if (is_huge) {
    std::string path = "/dev/hugepages" + std::string(name);
    ::unlink(path.c_str());
  } else {
    std::string path = std::string(name);
    shm_unlink(path.c_str());
  }
}

// ============================================================================
// Create 延遲
// ============================================================================

static void BM_Create_Regular(benchmark::State& state) {
  const char* name = "/bench_create_reg";
  for (auto _ : state) {
    // 清理不計時
    state.PauseTiming();
    cleanup_shm(name, false);
    state.ResumeTiming();

    auto shm = SharedMemory::create(name, 4096);
    benchmark::DoNotOptimize(shm);

    if (!shm.has_value()) {
      state.SkipWithError("Create failed");
      return;
    }
  }

  cleanup_shm(name, false);

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Create_Regular);

static void BM_Create_Huge(benchmark::State& state) {
  const char* name = "/bench_create_huge";
  const size_t size = 2 * 1024 * 1024;  // 2 MB
  for (auto _ : state) {
    // 清理不計時
    state.PauseTiming();
    cleanup_shm(name, true);
    state.ResumeTiming();

    auto shm = SharedMemory::create_huge(name, size);
    benchmark::DoNotOptimize(shm);

    if (!shm.has_value()) {
      state.SkipWithError("Create Huge failed");
      return;
    }
  }

  cleanup_shm(name, true);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Create_Huge);

// ============================================================================
// Open 延遲
// ============================================================================

// Regular
static void BM_Open_Regular(benchmark::State& state) {
  const char* name = "/bench_open_reg";
  const size_t size = 4096;

  // 建立一個 shard memeory
  cleanup_shm(name, false);
  auto creator = SharedMemory::create(name, size);
  if (!creator) {
    state.SkipWithError("Setup failed");
    return;
  }

  for (auto _ : state) {
    auto shm = SharedMemory::open(name);
    benchmark::DoNotOptimize(shm);

    if (!shm) {
      state.SkipWithError("Open failed");
      return;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Open_Regular);

/// 因爲 create 版本不會預先 Populate 可以測一下第一次訪問會比較久
static void BM_Open_And_FirstAccess_Regular(benchmark::State& state) {
  const char* name = "/bench_open_access";
  const size_t size = 1024 * 1024;  // 1MB

  cleanup_shm(name, false);
  auto creator = SharedMemory::create(name, size);

  if (!creator) {
    state.SkipWithError("Setup failed");
    return;
  }

  for (auto _ : state) {
    auto shm = SharedMemory::open(name);
    if (!shm) {
      state.SkipWithError("Open failed");
      return;
    }

    // 訪問第一個位元組（觸發 Page Fault）
    int64_t* data = shm->as<int64_t>();
    benchmark::DoNotOptimize(*data);
    benchmark::ClobberMemory();  // 防止編譯器快取優化
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Open_And_FirstAccess_Regular);

// Huge
static void BM_Open_Huge(benchmark::State& state) {
  const char* name = "/bench_open_huge";
  const size_t size = 1024 * 1024 * 2;

  // 建立一個 shard memeory
  cleanup_shm(name, true);
  auto creator = SharedMemory::create_huge(name, size);
  if (!creator) {
    state.SkipWithError("Setup huge failed");
    return;
  }

  for (auto _ : state) {
    auto shm = SharedMemory::open_huge(name);
    benchmark::DoNotOptimize(shm);

    if (!shm) {
      state.SkipWithError("Open huge failed");
      return;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Open_Huge);

// ============================================================================
// Memory Access (Hot Path)
// ============================================================================

static void BM_Access_Single_Regular(benchmark::State& state) {
  const char* name = "/bench_access_single";
  const size_t size = 4096;

  cleanup_shm(name, false);

  auto shm = SharedMemory::create(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();

  for (auto _ : state) {
    benchmark::DoNotOptimize(*data);  // 訪問該地址
    benchmark::ClobberMemory();       // 防止編譯器快取優化
  }
}
BENCHMARK(BM_Access_Single_Regular);

static void BM_Access_Single_Huge(benchmark::State& state) {
  const char* name = "/bench_access_single_huge";
  const size_t size = 2 * 1024 * 1024;

  cleanup_shm(name, false);

  auto shm = SharedMemory::create_huge(name, size);

  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();

  for (auto _ : state) {
    benchmark::DoNotOptimize(*data);  // 訪問該地址
    benchmark::ClobberMemory();       // 防止編譯器快取優化
  }
}
BENCHMARK(BM_Access_Single_Huge);

// ============================================================================
// 連續訪問
// ============================================================================

/// @brief Big Size with little Stride
static void BM_Access_SmallData_SmallStride_Regular(benchmark::State& state) {
  const char* name = "/bench_small_small_reg";
  const size_t size = 8 * 1024 * 1024;  // 8 MB

  cleanup_shm(name, false);

  auto shm = SharedMemory::create(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();
  const size_t num_elements = size / sizeof(int64_t);
  const size_t stride = 512;  // 每 4 KB

  // Initialize
  for (size_t i = 0; i < num_elements; i++) {
    data[i] = static_cast<int64_t>(i);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < num_elements; i += stride) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Access_SmallData_SmallStride_Regular);

static void BM_Access_SmallData_SmallStride_Huge(benchmark::State& state) {
  const char* name = "/bench_small_small_huge";
  const size_t size = 8 * 1024 * 1024;  // 8 MB

  cleanup_shm(name, true);

  auto shm = SharedMemory::create_huge(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();
  const size_t num_elements = size / sizeof(int64_t);
  const size_t stride = 512;

  for (size_t i = 0; i < num_elements; i++) {
    data[i] = static_cast<int64_t>(i);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < num_elements; i += stride) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Access_SmallData_SmallStride_Huge);

/// @brief Big Data with Little Stride
static void BM_Access_BigData_SmallStride_Regular(benchmark::State& state) {
  const char* name = "/bench_big_small_reg";
  const size_t size = 512 * 1024 * 1024;  // 512 MB

  cleanup_shm(name, false);

  auto shm = SharedMemory::create(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();
  const size_t num_elements = size / sizeof(int64_t);
  const size_t stride = 512;  // 4 KB

  // Initialize
  for (size_t i = 0; i < num_elements; i++) {
    data[i] = static_cast<int64_t>(i);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < num_elements; i += stride) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Access_BigData_SmallStride_Regular);

static void BM_Access_BigData_SmallStride_Huge(benchmark::State& state) {
  const char* name = "/bench_big_small_huge";
  const size_t size = 512 * 1024 * 1024;  // 512 MB

  cleanup_shm(name, true);

  auto shm = SharedMemory::create_huge(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();
  const size_t num_elements = size / sizeof(int64_t);
  const size_t stride = 512;  // 4 KB

  for (size_t i = 0; i < num_elements; i++) {
    data[i] = static_cast<int64_t>(i);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < num_elements; i += stride) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Access_BigData_SmallStride_Huge);

/// @brief Big Data with Little Stride
static void BM_Access_BigData_BigStride_Regular(benchmark::State& state) {
  const char* name = "/bench_big_big_reg";
  const size_t size = 512 * 1024 * 1024;  // 512 MB

  cleanup_shm(name, false);

  auto shm = SharedMemory::create(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();
  const size_t num_elements = size / sizeof(int64_t);
  const size_t stride = 2 * 1024;  // 2 MB

  // Initialize
  for (size_t i = 0; i < num_elements; i++) {
    data[i] = static_cast<int64_t>(i);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < num_elements; i += stride) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Access_BigData_BigStride_Regular);

static void BM_Access_BigData_BigStride_Huge(benchmark::State& state) {
  const char* name = "/bench_big_big_huge";
  const size_t size = 512 * 1024 * 1024;  // 512 MB

  cleanup_shm(name, true);

  auto shm = SharedMemory::create_huge(name, size);
  if (!shm.has_value()) {
    state.SkipWithError("Setup failed");
    return;
  }

  int64_t* data = shm->as<int64_t>();
  const size_t num_elements = size / sizeof(int64_t);
  const size_t stride = 2 * 1024;  // 2 MB

  for (size_t i = 0; i < num_elements; i++) {
    data[i] = static_cast<int64_t>(i);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < num_elements; i += stride) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Access_BigData_BigStride_Huge);
}  // namespace tx::ipc::bench
