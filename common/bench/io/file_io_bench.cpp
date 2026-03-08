#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "../util.hpp"
#include "onyx/bench/latency_recorder.hpp"
#include "onyx/io/file.hpp"
#include "onyx/io/mapped_file.hpp"
#include "onyx/sys/tsc_timer.hpp"

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace {

using namespace onyx::bench;
using namespace onyx::sys;
using namespace onyx::io;

struct BenchTempFile {
  std::string path;
  explicit BenchTempFile(size_t size) {
    static int counter = 0;
    path = "/tmp/bench_file_" + std::to_string(getpid()) + "_" + std::to_string(counter++) + ".dat";
    // 建立檔案並寫入假資料
    std::ofstream ofs(path, std::ios::binary);
    std::string pattern = "test data line\n";
    size_t written = 0;
    while (written < size) {
      size_t to_write = std::min(pattern.size(), size - written);
      ofs.write(pattern.data(), to_write);
      written += to_write;
    }
  }
  ~BenchTempFile() { ::unlink(path.c_str()); }
};

// ----------------------------------------------------------------------------
// 讀取資料 Strategies (策略模式)
// ----------------------------------------------------------------------------
// ifstream Getline (逐行讀取)
struct IfstreamGetlineStrategy {
  std::ifstream ifs;
  void init(const std::string& path) {
    ifs.open(path);
    if (!ifs) throw std::runtime_error("Failed to open file");
  }
  void prepare_run() {
    ifs.clear();
    ifs.seekg(0, std::ios::beg);
  }
  void run() {
    std::string line;
    while (std::getline(ifs, line)) {
      benchmark::DoNotOptimize(line);
    }
  }
};
// MappedFile Getline (逐行讀取)
struct MappedGetlineStrategy {
  std::optional<MappedFile> file;
  std::optional<MappedFile::Reader> reader;
  void init(const std::string& path) {
    auto res = MappedFile::open_read(path);
    if (!res) throw std::runtime_error("Failed to open mapped file");
    file = std::move(*res);
  }
  void prepare_run() {
    // 重新建立 reader 或 seek(0)
    reader = std::move(file->reader());
  }
  void run() {
    auto& r = *reader;
    while (!r.eof()) {
      benchmark::DoNotOptimize(r.read_line());
    }
  }
};
// ifstream Block Read (分塊讀取)
struct IfstreamBlockStrategy {
  std::ifstream ifs;
  std::vector<char> buffer;
  IfstreamBlockStrategy() : buffer(8192) {}
  void init(const std::string& path) {
    ifs.open(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("Failed to open file");
  }
  void prepare_run() {
    ifs.clear();
    ifs.seekg(0, std::ios::beg);
  }
  void run() {
    while (ifs.read(buffer.data(), buffer.size()) || ifs.gcount() > 0) {
      benchmark::DoNotOptimize(buffer.data());
    }
  }
};
// MappedFile Block Read (分塊讀取)
struct MappedBlockStrategy {
  std::optional<MappedFile> file;
  std::optional<MappedFile::Reader> reader;
  void init(const std::string& path) {
    auto res = MappedFile::open_read(path);
    if (!res) throw std::runtime_error("Failed to open mapped file");
    file = std::move(*res);
  }
  void prepare_run() { reader = file->reader(); }
  void run() {
    auto& r = *reader;
    while (!r.eof()) {
      benchmark::DoNotOptimize(r.read_bytes(8192));
    }
  }
};
// ifstream Read All (一次讀取)
struct IfstreamWholeStrategy {
  std::ifstream ifs;
  std::vector<char> buffer;
  size_t file_size = 0;
  void init(const std::string& path) {
    ifs.open(path, std::ios::binary | std::ios::ate);
    if (!ifs) throw std::runtime_error("Failed to open file");
    file_size = ifs.tellg();
    buffer.resize(file_size);
  }
  void prepare_run() {
    ifs.clear();
    ifs.seekg(0, std::ios::beg);
  }
  void run() {
    ifs.read(buffer.data(), file_size);
    benchmark::DoNotOptimize(buffer.data());
  }
};
// MappedFile Read All (一次讀取 / Memcpy)
struct MappedWholeStrategy {
  std::optional<MappedFile> file;
  std::vector<std::byte> buffer;  // 模擬 Copy 成本
  void init(const std::string& path) {
    auto res = MappedFile::open_read(path);
    if (!res) throw std::runtime_error("Failed to open mapped file");
    file = std::move(*res);
    buffer.resize(file->size());
  }
  void prepare_run() {
    // Mapped file 隨機存取不需要特別 rewind，只要 pointer 歸零
    // 這裡我們模擬的是 copy 過程，所以不需要特別重置 reader state
  }
  void run() {
    // 為了與 ifstream 公平比較 (Memory Bandwidth vs Syscall)，我們執行 memcpy
    std::memcpy(buffer.data(), file->raw_ptr(), file->size());
    benchmark::DoNotOptimize(buffer.data());
  }
};

// ----------------------------------------------------------------------------
// 寫入資料 Strategies
// ----------------------------------------------------------------------------
struct BenchData {
  std::vector<char> data;
  explicit BenchData(size_t size) : data(size) {
    // 填入一些無意義資料
    for (size_t i = 0; i < size; ++i) {
      data[i] = static_cast<char>(i % 256);
    }
  }
  std::span<const std::byte> as_bytes() const { return std::as_bytes(std::span(data)); }
};
// ofstream Write (Buffered)
struct IfstreamWriteStrategy {
  std::ofstream ofs;
  void init(const std::string& path, size_t size) {
    // Truncate existing file
    ofs.open(path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw std::runtime_error("Failed to open file");
  }
  void prepare_run() { ofs.seekp(0, std::ios::beg); }
  void run(const BenchData& source) {
    ofs.write(source.data.data(), source.data.size());
    // 注意: 這裡不呼叫 flush/sync，測試純寫入吞吐量 (含 User-space buffer copy)
    benchmark::DoNotOptimize(source.data.size());
  }
};
// File Write (Syscall)
struct FileWriteStrategy {
  std::optional<File> file;
  void init(const std::string& path, size_t size) {
    auto res = File::open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!res) throw std::runtime_error("Failed to open file");
    file = std::move(*res);
  }
  void prepare_run() { auto _ = file->seek(0, File::Whence::Begin); }
  void run(const BenchData& source) {
    auto res = file->write(source.as_bytes());
    if (!res) throw std::runtime_error("Write failed");
    benchmark::DoNotOptimize(*res);
  }
};
// MappedFile Write (Memcpy)
struct MappedWriteStrategy {
  std::optional<MappedFile> file;
  void init(const std::string& path, size_t size) {
    // MappedFile 寫入前必須先 Resize 到目標大小
    auto res = MappedFile::open_write(path, size);
    if (!res) throw std::runtime_error("Failed to open mapped file");
    file = std::move(*res);
  }
  void prepare_run() {
    // MappedFile 隨機存取，不需要 seek，只要 pointer 歸零
  }
  void run(const BenchData& source) {
    // 直接 Memcpy，這是最快的路徑
    std::memcpy(file->raw_ptr(), source.data.data(), source.data.size());
    benchmark::DoNotOptimize(file->raw_ptr());
  }
};

}  // namespace

template <typename Strategy>
static void BM_FileIO(benchmark::State& state) {
  size_t file_size = state.range(0);
  BenchTempFile temp_file(file_size);

  LatencyRecorder recorder(state.max_iterations);

  Strategy strategy;
  strategy.init(temp_file.path);  // Open file

  for (auto _ : state) {
    strategy.prepare_run();  // Rewind / Reset
    uint64_t t0 = TSCTimer::now();
    strategy.run();  // Action
    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
  }
  register_latency_stats(state, recorder);
  state.SetBytesProcessed(state.iterations() * file_size);
}

// ----------------------------------------------------------------------------
// Read Benchmark
// ----------------------------------------------------------------------------

// Line Reading (Small: 10KB, Medium: 1MB)
BENCHMARK_TEMPLATE(BM_FileIO, IfstreamGetlineStrategy)
    ->Name("Ifstream/Getline/10KB")
    ->Arg(10 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileIO, MappedGetlineStrategy)
    ->Name("MappedFile/Getline/10KB")
    ->Arg(10 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileIO, IfstreamGetlineStrategy)
    ->Name("Ifstream/Getline/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileIO, MappedGetlineStrategy)
    ->Name("MappedFile/Getline/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();

// Block Reading (Medium: 1MB)
BENCHMARK_TEMPLATE(BM_FileIO, IfstreamBlockStrategy)
    ->Name("Ifstream/BlockRead/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileIO, MappedBlockStrategy)
    ->Name("MappedFile/BlockRead/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();

// Whole Reading (Large: 10MB)
BENCHMARK_TEMPLATE(BM_FileIO, IfstreamWholeStrategy)
    ->Name("Ifstream/ReadAll/10MB")
    ->Arg(10 * 1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileIO, MappedWholeStrategy)
    ->Name("MappedFile/ReadAll/10MB")
    ->Arg(10 * 1024 * 1024)
    ->UseRealTime();

// ----------------------------------------------------------------------------
// Write benchmark
// ----------------------------------------------------------------------------

template <typename Strategy>
static void BM_FileWrite(benchmark::State& state) {
  size_t data_size = state.range(0);
  BenchTempFile temp_file(0);   // 建立空檔 (路徑佔位)
  BenchData source(data_size);  // 準備資料
  Strategy strategy;
  strategy.init(temp_file.path, data_size);
  LatencyRecorder recorder(state.max_iterations);
  for (auto _ : state) {
    strategy.prepare_run();
    uint64_t t0 = TSCTimer::now();
    strategy.run(source);
    uint64_t t1 = TSCTimer::now();
    recorder.record(t1 - t0);
  }
  register_latency_stats(state, recorder);
  state.SetBytesProcessed(state.iterations() * data_size);
}

// Small Write (4KB) - Log Entry Simulation
BENCHMARK_TEMPLATE(BM_FileWrite, IfstreamWriteStrategy)
    ->Name("Ifstream/Write/4KB")
    ->Arg(4096)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileWrite, FileWriteStrategy)
    ->Name("File(Syscall)/Write/4KB")
    ->Arg(4096)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileWrite, MappedWriteStrategy)
    ->Name("MappedFile/Write/4KB")
    ->Arg(4096)
    ->UseRealTime();
// Medium Write (1MB)
BENCHMARK_TEMPLATE(BM_FileWrite, IfstreamWriteStrategy)
    ->Name("Ifstream/Write/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileWrite, FileWriteStrategy)
    ->Name("File(Syscall)/Write/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileWrite, MappedWriteStrategy)
    ->Name("MappedFile/Write/1MB")
    ->Arg(1024 * 1024)
    ->UseRealTime();
// Large Write (10MB) - Throughput
BENCHMARK_TEMPLATE(BM_FileWrite, IfstreamWriteStrategy)
    ->Name("Ifstream/Write/10MB")
    ->Arg(10 * 1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileWrite, FileWriteStrategy)
    ->Name("File(Syscall)/Write/10MB")
    ->Arg(10 * 1024 * 1024)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_FileWrite, MappedWriteStrategy)
    ->Name("MappedFile/Write/10MB")
    ->Arg(10 * 1024 * 1024)
    ->UseRealTime();
