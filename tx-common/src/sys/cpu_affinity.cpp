#include "tx/sys/cpu_affinity.hpp"

#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <vector>

#include "tx/error.hpp"
#include "tx/io/buf_reader.hpp"
#include "tx/io/file.hpp"

namespace tx::sys {

// ----------------------------------------------------------------------------
// Helper Functions
// ----------------------------------------------------------------------------

namespace {

/// @brief 解析 CPU 範圍字串 (e.g., "0-3,8,12-15")
///
std::vector<size_t> parse_cpu_range(std::string_view range_str) {
  std::vector<size_t> cpus;
  std::string token;

  const char* ptr = range_str.data();
  const char* end = range_str.data() + range_str.size();

  while (ptr < end) {
    // 跳過分隔與空白
    while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
      ++ptr;
    }
    if (ptr == end) break;

    // 解析第一個數字
    size_t start_cpu = 0;
    auto res = std::from_chars(ptr, end, start_cpu);
    if (res.ptr == ptr) break;  // 解析失敗
    ptr = res.ptr;              // 移到後面

    // 檢查是否有範圍 '-'
    if (ptr < end && *ptr == '-') {
      ++ptr;
      size_t end_cpu = 0;
      res = std::from_chars(ptr, end, end_cpu);
      if (res.ptr == ptr) {
        break;  // 解析錯誤
      } else {
        ptr = res.ptr;
        for (size_t i = start_cpu; i <= end_cpu; ++i) {
          cpus.push_back(i);
        }
      }
    } else {  // 沒有範圍
      cpus.push_back(start_cpu);
    }
  }
  return cpus;
}

Result<std::vector<size_t>> read_available_cpus_from_sysfs() {
  std::vector<std::byte> bytes =
      TRY(io::File::open("/sys/devices/system/cpu/online", O_RDONLY)
              .and_then([](io::File f) {
                return io::BufReader::from_file(std::move(f));
              })
              .and_then([](io::BufReader r) { return r.read_to_end(); }));

  if (bytes.empty()) return {};

  std::string_view content(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());

  while (!content.empty() && std::isspace(content.back())) {
    content.remove_suffix(1);
  }

  if (content.empty()) {
    return {};
  }

  return parse_cpu_range(content);
}

}  // namespace

// ----------------------------------------------------------------------------
// Affinity Control
// ----------------------------------------------------------------------------

Result<> CPUAffinity::pin_to_cpu(size_t cpu_id) noexcept {
  if (!is_valid_cpu(cpu_id)) {
    return tx::fail(std::errc::invalid_argument, "CPU ID out of range");
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);         // 清空
  CPU_SET(cpu_id, &cpuset);  // 設置該 cpu id

  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
    return tx::fail(errno, "sched_setaffinity() failed");
  }

  return {};
}

Result<> CPUAffinity::pin_to_cpus(std::span<const size_t> cpu_ids) noexcept {
  if (cpu_ids.empty()) {
    return tx::fail(std::errc::invalid_argument, "Empty CPU list");
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);  // 清空

  for (size_t cpu_id : cpu_ids) {
    if (!is_valid_cpu(cpu_id)) {
      return tx::fail(std::errc::invalid_argument, "Invalid CPU ID in list");
    }
    CPU_SET(cpu_id, &cpuset);  // 設置該 cpu id
  }

  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
    return tx::fail(errno, "sched_setaffinity() failed");
  }

  return {};
}

Result<std::vector<size_t>> CPUAffinity::get_affinity() noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == -1) {
    return tx::fail(errno, "sched_getaffinity() failed");
  }

  std::vector<size_t> cpu_list;
  cpu_list.reserve(get_cpu_count());

  for (size_t i = 0; i < CPU_SETSIZE; ++i) {
    if (CPU_ISSET(i, &cpuset)) {
      cpu_list.push_back(i);
    }
  }

  return cpu_list;
}

Result<> CPUAffinity::clear_affinity() noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  for (size_t i = 0; i < get_cpu_count(); ++i) {
    CPU_SET(i, &cpuset);
  }

  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
    return tx::fail(errno, "sched_setaffinity failed");
  }

  return {};
}

// ----------------------------------------------------------------------------
// 系統資訊
// ----------------------------------------------------------------------------

size_t CPUAffinity::get_cpu_count() noexcept {
  long count = ::sysconf(_SC_NPROCESSORS_CONF);
  return (count == -1) ? 1 : static_cast<size_t>(count);
}

std::vector<size_t> CPUAffinity::get_available_cpus() noexcept {
  // 嘗試從 sysfs 讀取
  auto cpus = read_available_cpus_from_sysfs();

  if (cpus && !cpus->empty()) {
    return std::move(*cpus);
  }

  // Fallback: 返回 [0, cpu_count)
  std::vector<size_t> res;
  size_t count = get_cpu_count();
  res.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    res.push_back(i);
  }
  return res;
}

// ----------------------------------------------------------------------------
// 驗證
// ----------------------------------------------------------------------------

bool CPUAffinity::is_valid_cpu(size_t cpu_id) noexcept {
  return cpu_id < get_cpu_count();
}

bool CPUAffinity::is_cpu_available(size_t cpu_id) noexcept {
  if (!is_valid_cpu(cpu_id)) {
    return false;
  }

  auto available = get_available_cpus();
  return std::find(available.begin(), available.end(), cpu_id) !=
         available.end();
}

}  // namespace tx::sys
