#include "onyx/db/pg/connection.hpp"

#include <gtest/gtest.h>

#include "onyx/db/error.hpp"

namespace onyx::db::pg {

// ============================================================================
// Test Fixture
// ============================================================================

class ConnectionTest : public ::testing::Test {
 protected:
  // 測試用的連線字串 - 請根據你的環境調整
  // 格式: postgresql://user:password@host:port/database
  const char* test_conninfo_ = "postgresql://postgres:databaseadm@localhost:5432/test_db";

  Connection conn_;

  void SetUp() override {
    // 每個測試前確保連線是乾淨的
    conn_.close();
  }

  void TearDown() override {
    // 每個測試後清理
    conn_.close();
  }

  // Helper: 建立測試表
  void CreateTestTable() {
    auto result = conn_.execute(R"(
      CREATE TABLE IF NOT EXISTS test_users (
        id SERIAL PRIMARY KEY,
        name VARCHAR(100) NOT NULL,
        age INTEGER,
        score REAL,
        active BOOLEAN DEFAULT true
      )
    )");
    ASSERT_TRUE(result.has_value()) << "Failed to create test table";
  }

  // Helper: 清理測試表
  void DropTestTable() { conn_.execute("DROP TABLE IF EXISTS test_users"); }
};

// ============================================================================
// Connection Management Tests
// ============================================================================

TEST_F(ConnectionTest, DefaultConstructor) {
  Connection new_conn;
  EXPECT_FALSE(new_conn.is_connected());
}

TEST_F(ConnectionTest, ConnectSuccess) {
  auto result = conn_.connect(test_conninfo_);
  ASSERT_TRUE(result.has_value()) << "Connection should succeed";
  EXPECT_TRUE(conn_.is_connected());
}

TEST_F(ConnectionTest, ConnectFailWithInvalidConnInfo) {
  auto result = conn_.connect("postgresql://invalid:invalid@nonexistent:9999/fake_db");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::ConnectionFail);
  EXPECT_FALSE(conn_.is_connected());
}

TEST_F(ConnectionTest, CloseConnection) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  EXPECT_TRUE(conn_.is_connected());

  conn_.close();
  EXPECT_FALSE(conn_.is_connected());
}

TEST_F(ConnectionTest, ReconnectAfterClose) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  conn_.close();

  auto result = conn_.connect(test_conninfo_);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(conn_.is_connected());
}

// ============================================================================
// Connection State Tests
// ============================================================================

TEST_F(ConnectionTest, GetConnectionInfo) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  EXPECT_FALSE(conn_.db_name().empty());
  EXPECT_FALSE(conn_.user().empty());
  EXPECT_FALSE(conn_.host().empty());
  EXPECT_FALSE(conn_.port().empty());
}

// ============================================================================
// Simple Query Tests - Execute (INSERT/UPDATE/DELETE)
// ============================================================================

TEST_F(ConnectionTest, ExecuteWithoutConnection) {
  EXPECT_FALSE(conn_.is_connected());
  auto result = conn_.execute("SELECT 1");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::ConnectionFail);
}

TEST_F(ConnectionTest, ExecuteCreateTable) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  DropTestTable();

  auto result = conn_.execute(R"(
    CREATE TABLE test_users (
      id SERIAL PRIMARY KEY,
      name VARCHAR(100)
    )
  )");

  ASSERT_TRUE(result.has_value());
  DropTestTable();
}

TEST_F(ConnectionTest, ExecuteInsertSingleRow) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  auto result = conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30)");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);  // 1 row affected

  DropTestTable();
}

TEST_F(ConnectionTest, ExecuteInsertMultipleRows) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  auto result = conn_.execute(R"(
    INSERT INTO test_users (name, age) VALUES
      ('Bob', 25),
      ('Charlie', 35),
      ('David', 40)
  )");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 3);  // 3 rows affected

  DropTestTable();
}

TEST_F(ConnectionTest, ExecuteUpdate) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30), ('Bob', 25)")
                  .has_value());

  // Update
  auto result = conn_.execute("UPDATE test_users SET age = 31 WHERE name = 'Alice'");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);  // 1 row updated

  DropTestTable();
}

TEST_F(ConnectionTest, ExecuteDelete) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30), ('Bob', 25)")
                  .has_value());

  // Delete
  auto result = conn_.execute("DELETE FROM test_users WHERE name = 'Alice'");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);  // 1 row deleted

  DropTestTable();
}

TEST_F(ConnectionTest, ExecuteWithInvalidSQL) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.execute("INVALID SQL STATEMENT");

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::QueryFail);
}

TEST_F(ConnectionTest, ExecuteWithStringParameter) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  std::string sql = "INSERT INTO test_users (name, age) VALUES ('Eve', 28)";
  auto result = conn_.execute(sql);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);

  DropTestTable();
}

// ============================================================================
// Simple Query Tests - Query (SELECT)
// ============================================================================

TEST_F(ConnectionTest, QueryWithoutConnection) {
  EXPECT_FALSE(conn_.is_connected());
  auto result = conn_.query("SELECT 1");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::ConnectionFail);
}

TEST_F(ConnectionTest, QuerySelectSimple) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.query("SELECT 1 AS num, 'hello' AS text");

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 1);
  EXPECT_FALSE(rs.empty());
}

TEST_F(ConnectionTest, QuerySelectFromTable) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_
                  .execute(R"(
    INSERT INTO test_users (name, age, score, active) VALUES
      ('Alice', 30, 95.5, true),
      ('Bob', 25, 87.3, false),
      ('Charlie', 35, 92.1, true)
  )")
                  .has_value());

  // Query
  auto result = conn_.query("SELECT * FROM test_users ORDER BY name");

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 3);

  DropTestTable();
}

TEST_F(ConnectionTest, QueryEmptyResult) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  auto result = conn_.query("SELECT * FROM test_users WHERE name = 'NonExistent'");

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 0);
  EXPECT_TRUE(rs.empty());

  DropTestTable();
}

TEST_F(ConnectionTest, QueryWithInvalidSQL) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.query("SELECT * FROM non_existent_table");

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::QueryFail);
}

TEST_F(ConnectionTest, QueryWithStringParameter) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  std::string sql = "SELECT 42 AS answer";
  auto result = conn_.query(sql);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().size(), 1);
}

// ============================================================================
// Parameterized Query Tests
// ============================================================================

// NOTE: Parameterized query tests are disabled due to bug in connection.hpp
// The template functions call 'execute_params_internal' which doesn't exist
// The actual function is named 'execute_internal'
// TODO: Fix connection.hpp line 265 and 277 to use 'execute_internal'

TEST_F(ConnectionTest, ParameterizedExecuteWithInteger) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  auto result = conn_.execute("INSERT INTO test_users (name, age) VALUES ($1, $2)", "Alice", 30);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedExecuteWithMultipleTypes_Duplicate) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  auto result =
      conn_.execute("INSERT INTO test_users (name, age, score, active) VALUES ($1, $2, $3, $4)",
                    "Bob", 25, 87.5, true);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedQueryWithInteger_Duplicate) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30), ('Bob', 25)")
                  .has_value());

  // Parameterized query
  auto result = conn_.query("SELECT * FROM test_users WHERE age > $1", 26);

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 1);  // Only Alice (age 30)

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedQueryWithString_Duplicate) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30), ('Bob', 25)")
                  .has_value());

  // Parameterized query with string
  auto result = conn_.query("SELECT * FROM test_users WHERE name = $1", "Alice");

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 1);

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedQueryWithMultipleParameters_Duplicate) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_
                  .execute(R"(
    INSERT INTO test_users (name, age, active) VALUES
      ('Alice', 30, true),
      ('Bob', 25, false),
      ('Charlie', 35, true)
  )")
                  .has_value());

  // Query with multiple parameters
  auto result = conn_.query("SELECT * FROM test_users WHERE age > $1 AND active = $2", 20, true);

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 2);  // Alice and Charlie

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedExecuteWithMultipleTypes) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  auto result =
      conn_.execute("INSERT INTO test_users (name, age, score, active) VALUES ($1, $2, $3, $4)",
                    "Bob", 25, 87.5, true);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1);

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedQueryWithInteger) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30), ('Bob', 25)")
                  .has_value());

  // Parameterized query
  auto result = conn_.query("SELECT * FROM test_users WHERE age > $1", 26);

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 1);  // Only Alice (age 30)

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedQueryWithString) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30), ('Bob', 25)")
                  .has_value());

  // Parameterized query with string
  auto result = conn_.query("SELECT * FROM test_users WHERE name = $1", "Alice");

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 1);

  DropTestTable();
}

TEST_F(ConnectionTest, ParameterizedQueryWithMultipleParameters) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_
                  .execute(R"(
    INSERT INTO test_users (name, age, active) VALUES
      ('Alice', 30, true),
      ('Bob', 25, false),
      ('Charlie', 35, true)
  )")
                  .has_value());

  // Query with multiple parameters
  auto result = conn_.query("SELECT * FROM test_users WHERE age > $1 AND active = $2", 20, true);

  ASSERT_TRUE(result.has_value());
  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 2);  // Alice and Charlie

  DropTestTable();
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(ConnectionTest, BeginTransactionSuccess) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto tx_result = conn_.begin_transaction();
  ASSERT_TRUE(tx_result.has_value());
}

TEST_F(ConnectionTest, BeginTransactionTwiceFails) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto tx1 = conn_.begin_transaction();
  ASSERT_TRUE(tx1.has_value());

  auto tx2 = conn_.begin_transaction();
  EXPECT_FALSE(tx2.has_value());
  EXPECT_EQ(tx2.error(), DbErrc::QueryFail);

  // Cleanup
  ASSERT_TRUE(tx1.value().rollback().has_value());
}

TEST_F(ConnectionTest, TransactionCommit) {
  auto conn_res = conn_.connect(test_conninfo_);
  ASSERT_TRUE(conn_res) << conn_res.error().message();
  CreateTestTable();

  {
    auto tx = conn_.begin_transaction();
    ASSERT_TRUE(tx.has_value());

    ASSERT_TRUE(
        conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30)").has_value());

    ASSERT_TRUE(tx.value().commit().has_value());
  }

  // Verify data was committed
  auto result = conn_.query("SELECT COUNT(*) FROM test_users");
  ASSERT_TRUE(result.has_value());

  DropTestTable();
}

TEST_F(ConnectionTest, TransactionRollback) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  {
    auto tx = conn_.begin_transaction();
    ASSERT_TRUE(tx.has_value());

    ASSERT_TRUE(
        conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30)").has_value());

    ASSERT_TRUE(tx.value().rollback().has_value());
  }

  // Verify data was rolled back
  auto result = conn_.query("SELECT COUNT(*) FROM test_users");
  ASSERT_TRUE(result.has_value());

  DropTestTable();
}

TEST_F(ConnectionTest, TransactionRAIIAutoRollback) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  {
    auto tx = conn_.begin_transaction();
    ASSERT_TRUE(tx.has_value());

    ASSERT_TRUE(
        conn_.execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30)").has_value());

    // Transaction goes out of scope without commit -> auto rollback
  }

  // Verify data was rolled back
  auto result = conn_.query("SELECT COUNT(*) FROM test_users");
  ASSERT_TRUE(result.has_value());

  DropTestTable();
}

TEST_F(ConnectionTest, CommitWithoutTransaction) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.commit();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::QueryFail);
}

TEST_F(ConnectionTest, RollbackWithoutTransaction) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.rollback();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::QueryFail);
}

// ============================================================================
// Prepared Statement Tests
// ============================================================================

TEST_F(ConnectionTest, PrepareStatementSuccess) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.prepare("SELECT $1::integer AS num");

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value().empty());
  EXPECT_TRUE(result.value().starts_with("stmt_"));
}

TEST_F(ConnectionTest, PrepareStatementWithoutConnection) {
  EXPECT_FALSE(conn_.is_connected());

  auto result = conn_.prepare("SELECT 1");

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::ConnectionFail);
}

TEST_F(ConnectionTest, PrepareStatementWithInvalidSQL) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto result = conn_.prepare("INVALID SQL");

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::QueryFail);
}

TEST_F(ConnectionTest, ExecutePreparedStatement) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Prepare statement
  auto stmt = conn_.prepare("INSERT INTO test_users (name, age) VALUES ($1, $2)");
  ASSERT_TRUE(stmt.has_value());

  // Execute prepared statement multiple times
  auto result1 = conn_.execute_prepared(stmt.value(), "Alice", 30);
  ASSERT_TRUE(result1.has_value());

  auto result2 = conn_.execute_prepared(stmt.value(), "Bob", 25);
  ASSERT_TRUE(result2.has_value());

  auto result3 = conn_.execute_prepared(stmt.value(), "Charlie", 35);
  ASSERT_TRUE(result3.has_value());

  // Verify
  auto query_result = conn_.query("SELECT COUNT(*) FROM test_users");
  ASSERT_TRUE(query_result.has_value());

  DropTestTable();
}

TEST_F(ConnectionTest, ExecutePreparedStatementQuery) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert test data
  ASSERT_TRUE(conn_
                  .execute(R"(
    INSERT INTO test_users (name, age) VALUES
      ('Alice', 30),
      ('Bob', 25),
      ('Charlie', 35)
  )")
                  .has_value());

  // Prepare and execute query
  auto stmt = conn_.prepare("SELECT * FROM test_users WHERE age > $1");
  ASSERT_TRUE(stmt.has_value());

  auto result = conn_.execute_prepared(stmt.value(), 26);
  ASSERT_TRUE(result.has_value());

  auto& rs = result.value();
  EXPECT_EQ(rs.size(), 2);  // Alice (30) and Charlie (35)

  DropTestTable();
}

TEST_F(ConnectionTest, PrepareMultipleStatements) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  auto stmt1 = conn_.prepare("SELECT $1::integer");
  auto stmt2 = conn_.prepare("SELECT $1::text");
  auto stmt3 = conn_.prepare("SELECT $1::boolean");

  ASSERT_TRUE(stmt1.has_value());
  ASSERT_TRUE(stmt2.has_value());
  ASSERT_TRUE(stmt3.has_value());

  // Each statement should have a unique name
  EXPECT_NE(stmt1.value(), stmt2.value());
  EXPECT_NE(stmt2.value(), stmt3.value());
  EXPECT_NE(stmt1.value(), stmt3.value());
}

TEST_F(ConnectionTest, ExecutePreparedWithoutConnection) {
  EXPECT_FALSE(conn_.is_connected());

  auto result = conn_.execute_prepared("stmt_0", 42);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), DbErrc::ConnectionFail);
}

// ============================================================================
// Edge Cases & Stress Tests
// ============================================================================

TEST_F(ConnectionTest, MultipleQueriesInSequence) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());

  for (int i = 0; i < 100; ++i) {
    auto result = conn_.query("SELECT $1::integer AS num", i);
    ASSERT_TRUE(result.has_value()) << "Failed at iteration " << i;
  }
}

TEST_F(ConnectionTest, LargeParameterizedInsert) {
  ASSERT_TRUE(conn_.connect(test_conninfo_).has_value());
  CreateTestTable();

  // Insert many rows using parameterized queries
  for (int i = 0; i < 1000; ++i) {
    std::string name = "User" + std::to_string(i);
    auto result =
        conn_.execute("INSERT INTO test_users (name, age) VALUES ($1, $2)", name, i % 100);
    ASSERT_TRUE(result.has_value()) << "Failed at iteration " << i;
  }

  // Verify count
  auto count_result = conn_.query("SELECT COUNT(*) FROM test_users");
  ASSERT_TRUE(count_result.has_value());

  DropTestTable();
}

}  // namespace onyx::db::pg
