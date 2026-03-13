#ifndef ONYX_DB_PG_CONNECTION_HPP
#define ONYX_DB_PG_CONNECTION_HPP

#include <atomic>
#include <memory>
#include <vector>

#include "libpq-fe.h"
#include "onyx/db/error.hpp"
#include "onyx/db/pg/result_set.hpp"
#include "onyx/error.hpp"

namespace onyx::db::pg {

class Transaction;

class Connection {
 private:
  struct PgConnDeleter {
    void operator()(PGconn* conn) const noexcept {
      if (conn) {
        PQfinish(conn);
      }
    }
  };

  std::unique_ptr<PGconn, PgConnDeleter> conn_;

  // Statement cache
  mutable std::atomic<size_t> stmt_counter_{0};

  // Transcation
  bool in_transaction_{false};

 public:
  Connection() = default;

  // ----------------------------------------------------------------------------
  // State
  // ----------------------------------------------------------------------------
  bool is_connected() const noexcept;
  std::string_view db_name() const noexcept;
  std::string_view user() const noexcept;
  std::string_view pass() const noexcept;
  std::string_view host() const noexcept;
  std::string_view port() const noexcept;

  // ----------------------------------------------------------------------------
  // Connection Management
  // ----------------------------------------------------------------------------

  /// @brief 建立 Postgre 資料庫連線
  ///
  /// @param conninfo 連線字符串
  /// @return 成功或失敗
  ///
  /// 連線字符串格式如下:
  /// ```text
  /// postgresql://[userspec@][hostspec][/dbname][?paramspec]
  /// ```
  /// * userspec: `user[:password]`
  /// * hostspec: `[host][:port][,...]`
  /// * paramspec: `name=value[&...]`
  /// ```
  Result<void> connect(const char* conninfo) noexcept;

  /// @brief 關閉連線
  void close() noexcept;

  // ----------------------------------------------------------------------------
  // Simple Query
  // ----------------------------------------------------------------------------

  /// @brief 執行命令 (INSERT/UPDATE/DELETE)
  ///
  /// @param sql Query (C-like 字符串)
  /// @return 回傳變動行數
  Result<int> execute(const char* sql) const noexcept;

  /// @brief 執行命令 (INSERT/UPDATE/DELETE)
  ///
  /// @param sql Query (string)
  /// @return 回傳變動行數
  ///
  /// 為甚麼不直接使用 std::string_view 因為他無法保證 null-terminated
  Result<int> execute(std::string& sql) const noexcept;

  /// @brief 執行查詢 (SELECT)
  ///
  /// @param sql Query (C-like 字符串)
  /// @return 返回查詢結果
  Result<ResultSet> query(const char* sql) const noexcept;

  /// @brief 執行查詢 (SELECT)
  ///
  /// @param sql Query (string)
  /// @return 返回查詢結果
  ///
  /// 為甚麼不直接使用 std::string_view 因為他無法保證 null-terminated
  Result<ResultSet> query(std::string& sql) const noexcept;

  // ----------------------------------------------------------------------------
  // Parameterized Query
  // ----------------------------------------------------------------------------

  /// @brief 執行參數化命令
  ///
  /// 範例:
  /// ```cpp
  /// conn.execute("INSERT INTO users (name, age) VALUES ($1, $2)", "Alice", 30);
  /// ```
  template <typename... Args>
  Result<int> execute(const char* sql, Args&&... args) const noexcept;

  /// @brief 執行參數化查詢
  ///
  /// 範例:
  /// ```cpp
  /// auto rs = conn.query("SELECT * FROM users WHERE age > $1", 18);
  /// ```
  template <typename... Args>
  Result<ResultSet> query(const char* sql, Args&&... args) const noexcept;

  // ----------------------------------------------------------------------------
  // Transaction
  // ----------------------------------------------------------------------------
  /// @brief 開始交易
  Result<Transaction> begin_transaction() noexcept;

  /// @brief 提交交易
  Result<void> commit() noexcept;

  /// @brief 回滾交易
  Result<void> rollback() noexcept;

  // ----------------------------------------------------------------------------
  // Manual Prepared Statement
  // ----------------------------------------------------------------------------

  /// @brief 手動準備語句（用於迴圈中重複執行）
  ///
  /// 範例:
  /// ```cpp
  /// auto stmt_name = conn.prepare("SELECT * FROM users WHERE id = $1");
  /// for (int i = 0; i < 1000; ++i) {
  ///   conn.execute_prepared(stmt_name.value(), i);
  /// }
  /// ```
  Result<std::string> prepare(const char* sql) const noexcept;

  /// @brief 執行已準備的語句
  template <typename... Args>
  Result<ResultSet> execute_prepared(const std::string& stmt_name, Args&&... args) const noexcept;

 private:
  Result<ResultPtr> execute_internal(const char* sql) const noexcept;

  template <typename... Args>
  Result<ResultPtr> execute_internal(const char* sql, Args&&... args) const noexcept;

  std::string get_or_create_prepared_statement(const char* sql) const noexcept;

  template <typename T>
  std::string to_param_string(T&& value) const;

  friend class Transaction;
};

// ----------------------------------------------------------------------------
// RAII Transaction
// ----------------------------------------------------------------------------

class Transaction {
 private:
  Connection* conn_;
  bool committed_{false};

 public:
  // ----------------------------------------------------------------------------
  // Constructor
  // ----------------------------------------------------------------------------

  explicit Transaction(Connection* conn) noexcept : conn_(conn) {}

  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------

  ~Transaction() noexcept {
    if (!committed_ && conn_) {
      conn_->rollback();
    }
  }

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  Transaction(Transaction&& other) noexcept : conn_(other.conn_), committed_(other.committed_) {
    other.conn_ = nullptr;  // Prevent double rollback
  }

  Transaction& operator=(Transaction&& other) noexcept {
    if (this != &other) {
      conn_ = other.conn_;
      committed_ = other.committed_;
      other.conn_ = nullptr;  // Prevent double rollback
    }
    return *this;
  }

  // ----------------------------------------------------------------------------
  // Operation
  // ----------------------------------------------------------------------------

  Result<void> commit() {
    auto result = conn_->commit();
    if (result) {
      committed_ = true;
    }
    return result;
  }

  Result<void> rollback() {
    auto result = conn_->rollback();
    if (result) {
      committed_ = true;  // Mark as completed to prevent double rollback
    }
    return result;
  }
};

template <typename T>
std::string Connection::to_param_string(T&& value) const {
  using DecayT = std::decay_t<T>;
  if constexpr (std::is_same_v<DecayT, std::string>) {
    return value;
  } else if constexpr (std::is_same_v<DecayT, std::string_view>) {
    return std::string(value);
  } else if constexpr (std::is_same_v<DecayT, const char*> || std::is_same_v<DecayT, char*>) {
    return std::string(value);
  } else if constexpr (std::is_integral_v<DecayT>) {
    return std::to_string(value);
  } else if constexpr (std::is_floating_point_v<DecayT>) {
    return std::to_string(value);
  } else if constexpr (std::is_same_v<DecayT, bool>) {
    return value ? "true" : "false";
  } else {
    static_assert(always_false_v<T>, "Unsupported parameter type for PostgreSQL");
  }
}

template <typename... Args>
Result<ResultPtr> Connection::execute_internal(const char* sql, Args&&... args) const noexcept {
  if (!is_connected()) {
    return onyx::fail(DbErrc::ConnectionFail);
  }

  // 將所有參數轉換為字串
  std::vector<std::string> param_strings;
  param_strings.reserve(sizeof...(Args));
  (param_strings.push_back(to_param_string(std::forward<Args>(args))), ...);

  // 建立 const char* 陣列
  std::vector<const char*> param_values;
  param_values.reserve(param_strings.size());
  for (const auto& s : param_strings) {
    param_values.push_back(s.c_str());
  }

  ResultPtr res(PQexecParams(conn_.get(), sql, static_cast<int>(param_values.size()), nullptr,
                             param_values.data(), nullptr, nullptr, 0));
  ExecStatusType status = PQresultStatus(res.get());
  if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
    char* msg = PQerrorMessage(conn_.get());
    return onyx::fail(DbErrc::QueryFail, msg);
  }
  return res;
}

template <typename... Args>
Result<int> Connection::execute(const char* sql, Args&&... args) const noexcept {
  ResultPtr res = TRY(execute_internal(sql, std::forward<Args>(args)...));

  char* affected_rows_str = PQcmdTuples(res.get());
  if (!affected_rows_str || affected_rows_str[0] == '\0') {
    return 0;
  }
  int affected_rows = TRY(parse_value<int>(affected_rows_str));
  return affected_rows;
}

template <typename... Args>
Result<ResultSet> Connection::query(const char* sql, Args&&... args) const noexcept {
  ResultPtr res = TRY(execute_internal(sql, std::forward<Args>(args)...));
  return ResultSet(std::move(res));
}

template <typename... Args>
Result<ResultSet> Connection::execute_prepared(const std::string& stmt_name,
                                               Args&&... args) const noexcept {
  if (!is_connected()) {
    return onyx::fail(DbErrc::ConnectionFail);
  }
  std::vector<std::string> param_strings;
  param_strings.reserve(sizeof...(Args));
  (param_strings.push_back(to_param_string(std::forward<Args>(args))), ...);

  std::vector<const char*> param_values;
  param_values.reserve(param_strings.size());
  for (const auto& s : param_strings) {
    param_values.push_back(s.c_str());
  }

  ResultPtr res(PQexecPrepared(conn_.get(), stmt_name.c_str(),
                               static_cast<int>(param_values.size()), param_values.data(), nullptr,
                               nullptr, 0));
  ExecStatusType status = PQresultStatus(res.get());
  if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
    char* msg = PQerrorMessage(conn_.get());
    return onyx::fail(DbErrc::QueryFail, msg);
  }
  return ResultSet(std::move(res));
}

}  // namespace onyx::db::pg

#endif
