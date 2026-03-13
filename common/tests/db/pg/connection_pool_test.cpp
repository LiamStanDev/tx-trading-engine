#include "onyx/db/pg/connection_pool.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

namespace onyx::db::pg {

// ============================================================================
// Test Fixture
// ============================================================================

class ConnectionPoolTest : public ::testing::Test {
 protected:
  // 測試用的連線字串 - 請根據你的環境調整
  const char* test_conninfo_ = "postgresql://postgres:databaseadm@localhost:5432/test_db";

  void SetUp() override {
    // 每個測試前的準備工作
  }

  void TearDown() override {
    // 每個測試後的清理工作
  }
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(ConnectionPoolTest, ConstructorWithDefaultPoolSize) {
  ConnectionPool pool(test_conninfo_);

  // Pool should be created successfully
  // We can verify by acquiring a connection
  auto conn = pool.acquire();
  EXPECT_TRUE(conn->is_connected());
}

TEST_F(ConnectionPoolTest, ConstructorWithCustomPoolSize) {
  ConnectionPool pool(test_conninfo_, 5);

  // Verify we can acquire connections
  auto conn = pool.acquire();
  EXPECT_TRUE(conn->is_connected());
}

TEST_F(ConnectionPoolTest, ConstructorWithSingleConnection) {
  ConnectionPool pool(test_conninfo_, 1);

  auto conn = pool.acquire();
  EXPECT_TRUE(conn->is_connected());
}

// ============================================================================
// Acquire & Return Tests
// ============================================================================

TEST_F(ConnectionPoolTest, AcquireSingleConnection) {
  ConnectionPool pool(test_conninfo_, 3);

  auto conn = pool.acquire();
  ASSERT_TRUE(conn->is_connected());

  // Should be able to execute queries
  auto result = conn->query("SELECT 1");
  EXPECT_TRUE(result.has_value());
}

TEST_F(ConnectionPoolTest, AcquireMultipleConnections) {
  ConnectionPool pool(test_conninfo_, 3);

  auto conn1 = pool.acquire();
  auto conn2 = pool.acquire();
  auto conn3 = pool.acquire();

  EXPECT_TRUE(conn1->is_connected());
  EXPECT_TRUE(conn2->is_connected());
  EXPECT_TRUE(conn3->is_connected());
}

TEST_F(ConnectionPoolTest, ConnectionAutoReturnOnScopeExit) {
  ConnectionPool pool(test_conninfo_, 1);

  {
    auto conn = pool.acquire();
    EXPECT_TRUE(conn->is_connected());
    // Connection should be returned when conn goes out of scope
  }

  // Should be able to acquire again immediately
  auto conn2 = pool.acquire();
  EXPECT_TRUE(conn2->is_connected());
}

TEST_F(ConnectionPoolTest, AcquireAfterReturn) {
  ConnectionPool pool(test_conninfo_, 2);

  {
    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();

    EXPECT_TRUE(conn1->is_connected());
    EXPECT_TRUE(conn2->is_connected());

    // conn1 will be returned when it goes out of scope
  }

  // Should be able to acquire a new connection
  auto conn3 = pool.acquire();
  EXPECT_TRUE(conn3->is_connected());
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_F(ConnectionPoolTest, ConcurrentAcquireAndReturn) {
  ConnectionPool pool(test_conninfo_, 5);

  auto worker = [&pool]() {
    for (int i = 0; i < 10; ++i) {
      auto conn = pool.acquire();
      EXPECT_TRUE(conn->is_connected());

      // Simulate some work (using simple query instead of parameterized)
      auto result = conn->query("SELECT 1");
      EXPECT_TRUE(result.has_value());

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  };

  // Launch multiple threads
  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back(worker);
  }

  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(ConnectionPoolTest, BlockingAcquireWhenPoolExhausted) {
  ConnectionPool pool(test_conninfo_, 2);

  bool acquired = false;
  std::thread t;

  {
    // Acquire all connections in this scope
    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();

    // Try to acquire in another thread (should block)
    t = std::thread([&pool, &acquired]() {
      auto conn3 = pool.acquire();
      acquired = true;
      EXPECT_TRUE(conn3->is_connected());
    });

    // Give the thread time to start and block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(acquired);

    // conn1 and conn2 will be returned when they go out of scope
  }

  // Wait for the thread to acquire
  t.join();
  EXPECT_TRUE(acquired);
}

TEST_F(ConnectionPoolTest, MultipleThreadsWaitingForConnection) {
  ConnectionPool pool(test_conninfo_, 1);

  std::atomic<int> acquired_count{0};

  auto waiter = [&pool, &acquired_count]() {
    auto c = pool.acquire();
    acquired_count.fetch_add(1, std::memory_order_relaxed);
    EXPECT_TRUE(c->is_connected());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  };

  std::vector<std::thread> threads;

  {
    // Acquire the only connection in this scope
    auto conn = pool.acquire();

    // Launch multiple waiting threads
    for (int i = 0; i < 5; ++i) {
      threads.emplace_back(waiter);
    }

    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(acquired_count.load(), 0);

    // conn will be returned when it goes out of scope
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // All threads should have acquired eventually
  EXPECT_EQ(acquired_count.load(), 5);
}

// ============================================================================
// PooledConnection RAII Tests
// ============================================================================

TEST_F(ConnectionPoolTest, PooledConnectionDereference) {
  ConnectionPool pool(test_conninfo_, 1);

  auto conn = pool.acquire();

  // Test operator->
  EXPECT_TRUE(conn->is_connected());

  // Test operator*
  Connection& ref = *conn;
  EXPECT_TRUE(ref.is_connected());
}

// ============================================================================
// Functional Tests with Real Queries
// ============================================================================

TEST_F(ConnectionPoolTest, ExecuteQueriesFromPool) {
  ConnectionPool pool(test_conninfo_, 3);

  // Create test table
  {
    auto conn = pool.acquire();
    auto result = conn->execute(R"(
      CREATE TABLE IF NOT EXISTS pool_test_users (
        id SERIAL PRIMARY KEY,
        name VARCHAR(100),
        value INTEGER
      )
    )");
    ASSERT_TRUE(result.has_value());
  }

  // Insert data using multiple connections
  {
    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();

    ASSERT_TRUE(conn1->execute("INSERT INTO pool_test_users (name, value) VALUES ('Alice', 100)")
                    .has_value());
    ASSERT_TRUE(conn2->execute("INSERT INTO pool_test_users (name, value) VALUES ('Bob', 200)")
                    .has_value());
  }

  // Query data
  {
    auto conn = pool.acquire();
    auto result = conn->query("SELECT COUNT(*) FROM pool_test_users");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
  }

  // Cleanup
  {
    auto conn = pool.acquire();
    conn->execute("DROP TABLE IF EXISTS pool_test_users");
  }
}

TEST_F(ConnectionPoolTest, TransactionFromPooledConnection) {
  ConnectionPool pool(test_conninfo_, 1);

  // Create test table
  {
    auto conn = pool.acquire();
    auto result = conn->execute(R"(
      CREATE TABLE IF NOT EXISTS pool_tx_test (
        id SERIAL PRIMARY KEY,
        value INTEGER
      )
    )");
    ASSERT_TRUE(result.has_value());
  }

  // Test transaction
  {
    auto conn = pool.acquire();

    auto tx = conn->begin_transaction();
    ASSERT_TRUE(tx.has_value());

    ASSERT_TRUE(conn->execute("INSERT INTO pool_tx_test (value) VALUES (42)").has_value());

    ASSERT_TRUE(tx.value().commit().has_value());
  }

  // Verify
  {
    auto conn = pool.acquire();
    auto result = conn->query("SELECT COUNT(*) FROM pool_tx_test");
    ASSERT_TRUE(result.has_value());
  }

  // Cleanup
  {
    auto conn = pool.acquire();
    conn->execute("DROP TABLE IF EXISTS pool_tx_test");
  }
}

TEST_F(ConnectionPoolTest, PreparedStatementFromPooledConnection) {
  ConnectionPool pool(test_conninfo_, 1);

  // Create test table
  {
    auto conn = pool.acquire();
    auto result = conn->execute(R"(
      CREATE TABLE IF NOT EXISTS pool_prep_test (
        id SERIAL PRIMARY KEY,
        name VARCHAR(100),
        value INTEGER
      )
    )");
    ASSERT_TRUE(result.has_value());
  }

  // Use prepared statement
  {
    auto conn = pool.acquire();

    auto stmt = conn->prepare("INSERT INTO pool_prep_test (name, value) VALUES ($1, $2)");
    ASSERT_TRUE(stmt.has_value());

    for (int i = 0; i < 10; ++i) {
      std::string name = "User" + std::to_string(i);
      auto result = conn->execute_prepared(stmt.value(), name, i * 10);
      ASSERT_TRUE(result.has_value());
    }
  }

  // Verify
  {
    auto conn = pool.acquire();
    auto result = conn->query("SELECT COUNT(*) FROM pool_prep_test");
    ASSERT_TRUE(result.has_value());
  }

  // Cleanup
  {
    auto conn = pool.acquire();
    conn->execute("DROP TABLE IF EXISTS pool_prep_test");
  }
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(ConnectionPoolTest, HighConcurrencyStressTest) {
  ConnectionPool pool(test_conninfo_, 10);

  std::atomic<int> success_count{0};
  std::atomic<int> failure_count{0};

  auto worker = [&pool, &success_count, &failure_count]() {
    for (int i = 0; i < 50; ++i) {
      try {
        auto conn = pool.acquire();
        // Use simple query instead of parameterized
        auto result = conn->query("SELECT 42, 'test'");

        if (result.has_value()) {
          success_count.fetch_add(1, std::memory_order_relaxed);
        } else {
          failure_count.fetch_add(1, std::memory_order_relaxed);
        }
      } catch (...) {
        failure_count.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  // Launch many threads
  std::vector<std::thread> threads;
  for (int i = 0; i < 20; ++i) {
    threads.emplace_back(worker);
  }

  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }

  // All queries should succeed
  EXPECT_EQ(success_count.load(), 20 * 50);
  EXPECT_EQ(failure_count.load(), 0);
}

TEST_F(ConnectionPoolTest, RapidAcquireReleaseStressTest) {
  ConnectionPool pool(test_conninfo_, 5);

  auto worker = [&pool]() {
    for (int i = 0; i < 100; ++i) {
      {
        auto conn = pool.acquire();
        EXPECT_TRUE(conn->is_connected());
        // Immediately release
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace onyx::db::pg
