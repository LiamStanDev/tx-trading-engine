#include "tx/ipc/shared_memory.hpp"

#include <gtest/gtest.h>
#include <sys/mman.h>

namespace tx::ipc::test {

TEST(SharedMemoryTest, CreateAndOpen) {
  const char* name = "/test_shm_basic";
  ::shm_unlink(name);

  auto result = SharedMemory::create(name, 4096);
  ASSERT_TRUE(result.is_ok());

  auto& shm = *result;
  EXPECT_EQ(shm.size(), 4096);
  EXPECT_TRUE(shm.is_valid());

  // 寫入數據
  int* data = shm.as<int>();
  ASSERT_NE(data, nullptr);
  data[0] = 42;

  // 打開
  auto result2 = SharedMemory::open(name);
  ASSERT_TRUE(result2.is_ok());

  auto& shm2 = *result2;
  int* data2 = shm2.as<int>();
  EXPECT_EQ(data2[0], 42);
}

}  // namespace tx::ipc::test
