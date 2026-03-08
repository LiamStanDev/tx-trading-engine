#include "onyx/sys/cpu_affinity.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

namespace onyx::sys::test {

class CPUAffinityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto original_affinity_result = CPUAffinity::get_affinity();
    ASSERT_TRUE(original_affinity_result) << "Failed to get initial affinity";
    original_affinity_ = std::move(*original_affinity_result);
  }

  void TearDown() override {
    ASSERT_TRUE(CPUAffinity::pin_to_cpus(original_affinity_))
        << "Failed to restore initail affinity";
  }

  std::vector<size_t> original_affinity_;
};

// ----------------------------------------------------------------------------
// System Info
// ----------------------------------------------------------------------------

TEST_F(CPUAffinityTest, GetCPUCount_ReturnsPositive) {
  int count = CPUAffinity::get_cpu_count();

  EXPECT_GT(count, 0) << "CPU count should be positive";
  EXPECT_LE(count, 1024) << "CPU count exceeds reasonable limit";

  long sysconf_count = ::sysconf(_SC_NPROCESSORS_CONF);
  if (sysconf_count == -1) {
    EXPECT_EQ(count, static_cast<int>(sysconf_count));
  }
}

TEST_F(CPUAffinityTest, GetAvailableCPUs_ValidRange) {
  auto cpus = CPUAffinity::get_available_cpus();

  EXPECT_FALSE(cpus.empty()) << "Available CPUs should not be empty";

  int count = CPUAffinity::get_cpu_count();
  for (int cpu : cpus) {
    EXPECT_GE(cpu, 0) << "CPU ID should be non-negative";
    EXPECT_LT(cpu, count) << "CPU ID should be less than total count";
  }

  // 驗證沒有重複
  std::vector<size_t> sorted = cpus;
  std::sort(sorted.begin(), sorted.end());
  auto it = std::adjacent_find(sorted.begin(), sorted.end());
  EXPECT_EQ(it, sorted.end()) << "CPU list should not contain duplicates";
}

TEST_F(CPUAffinityTest, IsValidCPU_BoundaryCheck) {
  int count = CPUAffinity::get_cpu_count();

  // Valid cases
  EXPECT_TRUE(CPUAffinity::is_valid_cpu(0)) << "CPU 0 should be valid";
  EXPECT_TRUE(CPUAffinity::is_valid_cpu(count - 1)) << "Last CPU should be valid";

  // Invalid cases
  EXPECT_FALSE(CPUAffinity::is_valid_cpu(-1)) << "Negative CPU should be invalid";
  EXPECT_FALSE(CPUAffinity::is_valid_cpu(count)) << "CPU ID == count should be invalid";
  EXPECT_FALSE(CPUAffinity::is_valid_cpu(9999)) << "Large CPU ID should be invalid";
}

// ----------------------------------------------------------------------------
// Pin to single CPU
// ----------------------------------------------------------------------------

TEST_F(CPUAffinityTest, PinToCPU_Success) {
  auto available = CPUAffinity::get_available_cpus();
  ASSERT_FALSE(available.empty());

  int target_cpu = available[0];
  auto result = CPUAffinity::pin_to_cpu(target_cpu);

  ASSERT_TRUE(result) << "pin_to_cpu should successed: " << result.error().message();

  auto affinity = CPUAffinity::get_affinity();
  ASSERT_EQ(affinity->size(), 1) << "Should be pinned to exactly 1 CPU";
  EXPECT_EQ((*affinity)[0], target_cpu) << "Should be pinned to target CPU";
}

TEST_F(CPUAffinityTest, PinToCPU_InvalidCPU_ReturnsError) {
  // Test negative CPU
  auto result1 = CPUAffinity::pin_to_cpu(-1);
  ASSERT_FALSE(result1);
  EXPECT_EQ(result1.error(), std::errc::invalid_argument);

  // Test out-of-range CPU
  auto result2 = CPUAffinity::pin_to_cpu(9999);
  ASSERT_FALSE(result2);
  EXPECT_EQ(result2.error(), std::errc::invalid_argument);
}

TEST_F(CPUAffinityTest, PinToCPU_ThreadLocal) {
  auto available = CPUAffinity::get_available_cpus();
  ASSERT_GE(available.size(), 2) << "Need at least 2 CPUs for this test";

  int main_cpu = available[0];
  int thread_cpu = available[1];

  // 主線程綁到 CPU 0
  ASSERT_TRUE(CPUAffinity::pin_to_cpu(main_cpu));

  // 子線程綁到 CPU 1
  std::vector<size_t> thread_affinity;
  std::thread t([thread_cpu, &thread_affinity]() {
    auto result = CPUAffinity::pin_to_cpu(thread_cpu);
    ASSERT_TRUE(result);

    auto aff = CPUAffinity::get_affinity();
    if (aff) {
      thread_affinity = *aff;
    }
  });
  t.join();

  // 驗證主線程的 affinity 沒有被改變
  auto main_affinity = CPUAffinity::get_affinity();
  ASSERT_TRUE(main_affinity);
  EXPECT_EQ((*main_affinity)[0], main_cpu) << "Main thread affinity should not change";

  // 驗證子線程成功綁定
  ASSERT_EQ(thread_affinity.size(), 1);
  EXPECT_EQ(thread_affinity[0], thread_cpu) << "Child thread should be pinned to different CPU";
}

// ----------------------------------------------------------------------------
// Pin to Multiple CPUs
// ----------------------------------------------------------------------------

TEST_F(CPUAffinityTest, PinToCPUs_Success) {
  auto available = CPUAffinity::get_available_cpus();
  ASSERT_GE(available.size(), 2) << "Need at least 2 CPUs for this test";

  std::vector<size_t> targets = {available[0], available[1]};
  auto result = CPUAffinity::pin_to_cpus(targets);

  ASSERT_TRUE(result) << "pin_to_cpus should succeed: " << result.error().message();

  auto affinity = CPUAffinity::get_affinity();
  ASSERT_TRUE(affinity);
  EXPECT_EQ(affinity->size(), 2) << "Should be pinned to 2 CPUs";

  // 驗證包含目標 CPUs
  EXPECT_NE(std::find(affinity->begin(), affinity->end(), targets[0]), affinity->end());
  EXPECT_NE(std::find(affinity->begin(), affinity->end(), targets[1]), affinity->end());
}

TEST_F(CPUAffinityTest, PinToCPUs_EmptyList_ReturnsError) {
  std::vector<size_t> empty;
  auto result = CPUAffinity::pin_to_cpus(empty);

  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST_F(CPUAffinityTest, PinToCPUs_InvalidCPU_ReturnsError) {
  auto available = CPUAffinity::get_available_cpus();
  ASSERT_FALSE(available.empty());

  // 包含無效 CPU ID
  std::vector<size_t> invalid = {available[0], 9999};
  auto result = CPUAffinity::pin_to_cpus(invalid);

  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

// ----------------------------------------------------------------------------
// Clear
// ----------------------------------------------------------------------------

TEST_F(CPUAffinityTest, ClearAffinity_RestoresAllCPUs) {
  auto available = CPUAffinity::get_available_cpus();
  ASSERT_FALSE(available.empty());

  // 先綁到單一 CPU
  ASSERT_TRUE(CPUAffinity::pin_to_cpu(available[0]));

  // 驗證確實綁定
  auto pinned = CPUAffinity::get_affinity();
  ASSERT_TRUE(pinned);
  EXPECT_EQ(pinned->size(), 1);

  // 清除 affinity
  auto result = CPUAffinity::clear_affinity();
  ASSERT_TRUE(result) << "clear_affinity should succeed: " << result.error().message();

  // 驗證恢復到所有 CPU
  auto affinity = CPUAffinity::get_affinity();
  ASSERT_TRUE(affinity);
  EXPECT_EQ(affinity->size(), CPUAffinity::get_cpu_count()) << "Should be able to run on all CPUs";
}

}  // namespace onyx::sys::test
