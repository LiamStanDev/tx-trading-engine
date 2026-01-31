#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "../util.hpp"
#include "tx/io/buf_reader.hpp"
#include "tx/io/file.hpp"
#include "tx/sys/tsc_timer.hpp"

using tx::sys::TSCTimer;

namespace tx::io::bench {

// ============================================================================
// 測試資料準備
// ============================================================================

namespace {

// 建立測試檔案
std::string create_test_file(size_t size,
                             const std::string& pattern = "test data line\n") {
  static int counter = 0;
  std::string path = "/tmp/bench_file_" + std::to_string(counter++) + ".dat";

  std::ofstream ofs(path, std::ios::binary);
  size_t written = 0;
  while (written < size) {
    size_t to_write = std::min(pattern.size(), size - written);
    ofs.write(pattern.data(), to_write);
    written += to_write;
  }
  ofs.close();

  return path;
}

// 清理測試檔案
void cleanup_file(const std::string& path) { ::unlink(path.c_str()); }

}  // namespace

// ============================================================================
// Group 1: 讀取延遲比較 - 小檔案行導向 (10KB, ~666 行)
// ============================================================================

static void BM_BufReader_ReadLines_Small(benchmark::State& state) {
  std::string path = create_test_file(10 * 1024);
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    auto file = File::open(path, O_RDONLY);
    if (!file) {
      state.SkipWithError("Failed to open file");
      return;
    }

    auto reader = BufReader::from_file(std::move(*file));
    auto lines = reader.read_lines();

    uint64_t t1 = TSCTimer::now();

    if (!lines) {
      state.SkipWithError("Failed to read lines");
      return;
    }

    recorder.record(t1 - t0);
    benchmark::DoNotOptimize(lines->size());
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 10 * 1024);
}
BENCHMARK(BM_BufReader_ReadLines_Small);

static void BM_ifstream_Getline_Small(benchmark::State& state) {
  std::string path = create_test_file(10 * 1024);
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    std::ifstream ifs(path);
    if (!ifs) {
      state.SkipWithError("Failed to open file");
      return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
      lines.push_back(std::move(line));
    }

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
    benchmark::DoNotOptimize(lines.size());
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 10 * 1024);
}
BENCHMARK(BM_ifstream_Getline_Small);

// ============================================================================
// Group 2: 讀取延遲比較 - 中檔案塊讀取 (1MB)
// ============================================================================

static void BM_BufReader_Read_Medium(benchmark::State& state) {
  std::string path = create_test_file(1 * 1024 * 1024);
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    auto file = File::open(path, O_RDONLY);
    if (!file) {
      state.SkipWithError("Failed to open file");
      return;
    }

    auto reader = BufReader::from_file(std::move(*file));
    std::vector<std::byte> buffer(8192);
    size_t total = 0;

    while (true) {
      auto result = reader.read(buffer);
      if (!result || *result == 0) break;
      total += *result;
    }

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
    benchmark::DoNotOptimize(total);
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 1 * 1024 * 1024);
}
BENCHMARK(BM_BufReader_Read_Medium);

static void BM_ifstream_Read_Medium(benchmark::State& state) {
  std::string path = create_test_file(1 * 1024 * 1024);
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      state.SkipWithError("Failed to open file");
      return;
    }

    std::vector<char> buffer(8192);
    size_t total = 0;

    while (ifs.read(buffer.data(), buffer.size()) || ifs.gcount() > 0) {
      total += ifs.gcount();
    }

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
    benchmark::DoNotOptimize(total);
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 1 * 1024 * 1024);
}
BENCHMARK(BM_ifstream_Read_Medium);

// ============================================================================
// Group 3: 讀取延遲比較 - 大檔案整讀 (10MB)
// ============================================================================

static void BM_BufReader_ReadToEnd_Large(benchmark::State& state) {
  std::string path = create_test_file(10 * 1024 * 1024);
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    auto file = File::open(path, O_RDONLY);
    if (!file) {
      state.SkipWithError("Failed to open file");
      return;
    }

    auto reader = BufReader::from_file(std::move(*file));
    auto data = reader.read_to_end();

    uint64_t t1 = TSCTimer::now();

    if (!data) {
      state.SkipWithError("Failed to read");
      return;
    }

    recorder.record(t1 - t0);
    benchmark::DoNotOptimize(data->size());
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 10 * 1024 * 1024);
}
BENCHMARK(BM_BufReader_ReadToEnd_Large);

static void BM_ifstream_ReadAll_Large(benchmark::State& state) {
  std::string path = create_test_file(10 * 1024 * 1024);
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) {
      state.SkipWithError("Failed to open file");
      return;
    }

    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    ifs.read(buffer.data(), size);

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
    benchmark::DoNotOptimize(buffer.size());
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 10 * 1024 * 1024);
}
BENCHMARK(BM_ifstream_ReadAll_Large);

// ============================================================================
// Group 4: 寫入延遲比較 - 小塊頻繁寫入 (4KB x 1000 次 = 4MB)
// ============================================================================

static void BM_File_Write_SmallChunks(benchmark::State& state) {
  std::string path = "/tmp/bench_write_file.dat";
  std::string data(4096, 'X');
  auto bytes = std::as_bytes(std::span(data));
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    auto file = File::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file) {
      state.SkipWithError("Failed to open file");
      return;
    }

    for (int i = 0; i < 1000; ++i) {
      auto result = file->write(bytes);
      if (!result) {
        state.SkipWithError("Failed to write");
        return;
      }
    }

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 4096 * 1000);
}
BENCHMARK(BM_File_Write_SmallChunks);

static void BM_ofstream_Write_SmallChunks(benchmark::State& state) {
  std::string path = "/tmp/bench_write_ofstream.dat";
  std::string data(4096, 'X');
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
      state.SkipWithError("Failed to open file");
      return;
    }

    for (int i = 0; i < 1000; ++i) {
      ofs.write(data.data(), data.size());
    }

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 4096 * 1000);
}
BENCHMARK(BM_ofstream_Write_SmallChunks);

// ============================================================================
// Group 5: 寫入延遲比較 - 大塊寫入 (1MB)
// ============================================================================

static void BM_File_Write_LargeChunk(benchmark::State& state) {
  std::string path = "/tmp/bench_write_large.dat";
  std::string data(1024 * 1024, 'Y');
  auto bytes = std::as_bytes(std::span(data));
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    auto file = File::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file) {
      state.SkipWithError("Failed to open file");
      return;
    }

    auto result = file->write(bytes);

    uint64_t t1 = TSCTimer::now();

    if (!result) {
      state.SkipWithError("Failed to write");
      return;
    }

    recorder.record(t1 - t0);
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 1024 * 1024);
}
BENCHMARK(BM_File_Write_LargeChunk);

static void BM_ofstream_Write_LargeChunk(benchmark::State& state) {
  std::string path = "/tmp/bench_write_large_ofs.dat";
  std::string data(1024 * 1024, 'Y');
  LatencyRecorder recorder(kBenchmarkIterationSize);

  for (auto _ : state) {
    uint64_t t0 = TSCTimer::now();

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
      state.SkipWithError("Failed to open file");
      return;
    }

    ofs.write(data.data(), data.size());

    uint64_t t1 = TSCTimer::now();

    recorder.record(t1 - t0);
  }

  auto stats = recorder.compute_stats();
  report_latency_stats(state, stats);

  cleanup_file(path);
  state.SetBytesProcessed(state.iterations() * 1024 * 1024);
}
BENCHMARK(BM_ofstream_Write_LargeChunk);

}  // namespace tx::io::bench
