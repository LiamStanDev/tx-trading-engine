#include "onyx/core/object_pool.hpp"

#include <gtest/gtest.h>

using namespace onyx::core;

namespace {

struct LifecycleObject {
  inline static int constructed_count{0};
  inline static int destructed_count{0};
  int id;

  explicit LifecycleObject(int i) noexcept : id(i) { ++constructed_count; }
  ~LifecycleObject() noexcept { ++destructed_count; }
  static void reset() noexcept {
    constructed_count = 0;
    destructed_count = 0;
  }
};

// ----------------------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------------------
class ObjectPoolTest : public ::testing::Test {
 protected:
  void SetUp() override { LifecycleObject::reset(); }
};

// ----------------------------------------------------------------------------
// Allocation & Deallocation
// ----------------------------------------------------------------------------

TEST_F(ObjectPoolTest, BasicAllocation) {
  ObjectPool<int, 2> pool;

  int* p1 = pool.allocate(10);
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(*p1, 10);

  int* p2 = pool.allocate(20);
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(*p2, 20);

  int* p3 = pool.allocate(30);
  ASSERT_EQ(p3, nullptr);

  EXPECT_TRUE(pool.full());
  EXPECT_EQ(pool.size(), 2);
}

TEST_F(ObjectPoolTest, LIFOReuseStrategy) {
  ObjectPool<int, 3> pool;

  // 全部分配出去
  int* p1 = pool.allocate(10);
  int* p2 = pool.allocate(20);
  int* p3 = pool.allocate(30);

  // 釋放一個
  pool.deallocate(p2);

  // 下一個應該要拿到剛試放的地址
  int* p4 = pool.allocate(40);
  ASSERT_NE(p4, nullptr);
  EXPECT_EQ(p4, p2);
  EXPECT_EQ(*p4, 40);

  // 釋放 p1, p3
  pool.deallocate(p1);
  pool.deallocate(p3);

  // 下次分配應拿到 p3
  int* p5 = pool.allocate(5);
  ASSERT_NE(p5, nullptr);
  EXPECT_EQ(p5, p3);
}

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

TEST_F(ObjectPoolTest, ObjectLifecycle) {
  ObjectPool<LifecycleObject, 5> pool;
  LifecycleObject* o1 = pool.allocate(1);
  LifecycleObject* o2 = pool.allocate(2);
  EXPECT_EQ(LifecycleObject::constructed_count, 2);

  pool.deallocate(o1);
  EXPECT_EQ(LifecycleObject::destructed_count, 1);
  pool.deallocate(o2);
  EXPECT_EQ(LifecycleObject::destructed_count, 2);
}

TEST_F(ObjectPoolTest, PoolDestructionCleansUpAllocatedObjects) {
  {
    ObjectPool<LifecycleObject, 5> pool;
    LifecycleObject* o1 = pool.allocate(1);
    LifecycleObject* o2 = pool.allocate(2);
  }
  EXPECT_EQ(LifecycleObject::constructed_count, 2);
  EXPECT_EQ(LifecycleObject::destructed_count, 2);
}

// ----------------------------------------------------------------------------
// Corner
// ----------------------------------------------------------------------------
TEST_F(ObjectPoolTest, PointerValidation) {
  ObjectPool<double, 2> pool;
  double* p1 = pool.allocate(1.0);

  // 測試 nullptr
  pool.deallocate(nullptr);  // 應該安全返回

  // 測試外部指針 (不屬於此 Pool)
  double outside_val = 5.0;
  pool.deallocate(&outside_val);  // 應該被邊界檢查攔截，不會 Crash

  EXPECT_EQ(pool.size(), 1);  // 狀態不應改變
}

TEST_F(ObjectPoolTest, CapacityOneEdgeCase) {
  ObjectPool<int, 1> pool;

  int* ptr = pool.allocate(22);
  EXPECT_EQ(pool.size(), 1);
  EXPECT_EQ(pool.available(), 0);

  pool.deallocate(ptr);
  EXPECT_EQ(pool.size(), 0);
  EXPECT_EQ(pool.available(), 1);

  ptr = pool.allocate(22);
  ptr = pool.allocate(23);
  EXPECT_EQ(pool.size(), 1);
  EXPECT_EQ(pool.available(), 0);
  EXPECT_EQ(ptr, nullptr);
}

TEST_F(ObjectPoolTest, AllocateDeallocateFullCycle) {
  ObjectPool<int, 3> pool;

  int* p1 = pool.allocate(22);
  int* p2 = pool.allocate(22);
  int* p3 = pool.allocate(22);

  pool.deallocate(p1);
  pool.deallocate(p2);
  pool.deallocate(p3);

  p1 = pool.allocate(22);
  p2 = pool.allocate(22);
  p3 = pool.allocate(22);

  pool.deallocate(p1);
  pool.deallocate(p2);
  pool.deallocate(p3);
}

TEST_F(ObjectPoolTest, StateCounterConsistency) {
  ObjectPool<int, 3> pool;
  int* p1 = pool.allocate(22);
  EXPECT_EQ(pool.size() + pool.available(), pool.capacity());
  int* p2 = pool.allocate(22);
  EXPECT_EQ(pool.size() + pool.available(), pool.capacity());
  int* p3 = pool.allocate(22);
  EXPECT_EQ(pool.size() + pool.available(), pool.capacity());

  pool.deallocate(p3);
  EXPECT_EQ(pool.size() + pool.available(), pool.capacity());
  pool.deallocate(p2);
  EXPECT_EQ(pool.size() + pool.available(), pool.capacity());
  pool.deallocate(p1);
  EXPECT_EQ(pool.size() + pool.available(), pool.capacity());
}

}  // namespace
