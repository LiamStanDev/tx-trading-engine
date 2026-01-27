#include "tx/ipc/shared_memory.hpp"

#include <gtest/gtest.h>
#include <sys/mman.h>

#include <cstdint>

#include "tx/error.hpp"

namespace tx::ipc::test {

/// 測試非 POC 類型違反 Concept
static_assert(SharedMemoryCompatible<int>);
static_assert(SharedMemoryCompatible<double>);
static_assert(!SharedMemoryCompatible<std::string>);

// ============================================================================
// 正常功能
// ============================================================================
TEST(SharedMemoryTest, CreateAndOpen) {
  const char* name = "/test_shm_basic";
  ::shm_unlink(name);

  auto result = SharedMemory::create(name, 4096);
  ASSERT_TRUE(result.has_value());

  auto& shm = *result;
  EXPECT_EQ(shm.size(), 4096);
  EXPECT_TRUE(shm.is_valid());

  // 寫入數據
  int* data = shm.as<int>();
  ASSERT_NE(data, nullptr);
  data[0] = 42;

  // 打開
  auto result2 = SharedMemory::open(name);
  ASSERT_TRUE(result2.has_value());

  auto& shm2 = *result2;
  int* data2 = shm2.as<int>();
  EXPECT_EQ(data2[0], 42);
}

TEST(SharedMemoryTest, AlignmentCheck) {
  const char* name = "/test_shm_align";
  ::shm_unlink(name);

  auto result = SharedMemory::create(name, 4096);

  ASSERT_TRUE(result);

  int64_t* ptr = result->as<int64_t>();

  ASSERT_NE(ptr, nullptr);

  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  EXPECT_EQ(addr % alignof(int64_t), 0);

  *ptr = 0x123456789ABCDEF0;
  EXPECT_EQ(*ptr, 0x123456789ABCDEF0);
}

// 需要權限
// TEST(SharedMemoryTest, HugePageCreateAndOpen) {
//   const char* name = "/test_shm_huge";
//   std::string path = "/dev/hugepages" + std::string(name);
//   ::unlink(path.c_str());
//
//   size_t size = 1024 * 1024 * 10;  // 10 BM
//   auto shm1 = SharedMemory::create_huge(name, size);
//
//   ASSERT_TRUE(shm1.has_value()) << "errno: " << shm1.error();
//
//   EXPECT_GE(shm1->size(), size);
//   EXPECT_TRUE(shm1->is_valid());
//
//   int* data = shm1->as<int>();
//   ASSERT_NE(data, nullptr);
//   *data = 999;
//   EXPECT_EQ(*data, 999);
//
//   auto shm2 = SharedMemory::open_huge(name);
//   ASSERT_TRUE(shm2.has_value());
//   EXPECT_EQ(shm2->size(), size);
//   EXPECT_TRUE(shm2->is_valid());
//
//   // 讀寫數據
//   int* data2 = shm2->as<int>();
//   ASSERT_NE(data2, nullptr);
//   EXPECT_EQ(*data2, 999);
//   *data2 = 42;
//
//   EXPECT_EQ(*data, 42);
// }

// ============================================================================
// 輸入驗証
// ============================================================================
TEST(SharedMemoryTest, InvalidName) {
  const char* name = "invalid_shm";

  auto result = SharedMemory::create(name, 4096);
  ASSERT_FALSE(result.has_value());

  EXPECT_EQ(result.error(), std::errc::invalid_argument);
  EXPECT_STREQ(ErrorRegistry::get_last_error().message,
               "SHM should start with '/'");
}

TEST(SharedMemoryTest, ZeroSize) {
  const char* name = "/zero_size_shm";
  ::shm_unlink(name);

  auto result = SharedMemory::create(name, 0);
  ASSERT_FALSE(result.has_value());

  EXPECT_EQ(result.error(), std::errc::invalid_argument);
  EXPECT_STREQ(ErrorRegistry::get_last_error().message, "Invalid size");
}

// ============================================================================
// 邊界條件
// ============================================================================
TEST(SharedMemoryTest, DoubleCreate) {
  const char* name = "/double_create_shm";
  ::shm_unlink(name);

  auto result = SharedMemory::create(name, 4096);
  ASSERT_TRUE(result.has_value());

  auto result2 = SharedMemory::create(name, 4096);
  ASSERT_FALSE(result2.has_value());

  EXPECT_EQ(result2.error(), std::errc::file_exists);
}

TEST(SharedMemoryTest, LargeSize) {
  const char* name = "/test_shm_large";
  ::shm_unlink(name);

  size_t large_size = 1ULL * 1024 * 1024 * 1024;  // 1 GB

  auto result = SharedMemory::create(name, large_size);
  if (result.has_value()) {
    EXPECT_EQ(result->size(), large_size);
    EXPECT_TRUE(result->is_valid());
  } else {
    EXPECT_EQ(result.error(), std::errc::not_enough_memory);
  }
}

// ============================================================================
// RAII
// ============================================================================
TEST(SharedMemoryTest, MoveConstruction) {
  const char* name = "/test_shm_move";
  ::shm_unlink(name);

  auto shm1 = SharedMemory::create(name, 4096);
  ASSERT_TRUE(shm1.has_value());

  void* original_addr = shm1->data();
  ASSERT_NE(original_addr, nullptr);

  SharedMemory shm2 = std::move(*shm1);

  EXPECT_EQ(shm2.data(), original_addr);
  EXPECT_EQ(shm2.size(), 4096);
  EXPECT_TRUE(shm2.is_valid());

  EXPECT_EQ(shm1->data(), nullptr);
  EXPECT_EQ(shm1->size(), 0);
  EXPECT_FALSE(shm1->is_valid());
}

TEST(SharedMemoryTest, MoveAssignment) {
  ::shm_unlink("/test_shm_move1");
  ::shm_unlink("/test_shm_move2");

  auto shm1 = SharedMemory::create("/test_shm_move1", 4096);
  auto shm2 = SharedMemory::create("/test_shm_move2", 8192);
  ASSERT_TRUE(shm1.has_value());
  ASSERT_TRUE(shm2.has_value());

  void* addr1 = shm1->data();

  *shm2 = std::move(*shm1);

  EXPECT_EQ(shm2->data(), addr1);
  EXPECT_EQ(shm2->size(), 4096);
  EXPECT_EQ(shm2->name(), "/test_shm_move1");

  EXPECT_FALSE(shm1->is_valid());
}

}  // namespace tx::ipc::test
