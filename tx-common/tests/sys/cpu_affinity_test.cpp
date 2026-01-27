#include "tx/sys/cpu_affinity.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

namespace tx::sys::test {

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

}  // namespace tx::sys::test
