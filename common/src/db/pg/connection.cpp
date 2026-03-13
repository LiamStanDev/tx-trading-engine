#include "onyx/db/pg/connection.hpp"

#include <libpq-fe.h>

#include "onyx/db/error.hpp"
#include "onyx/error.hpp"
#include "onyx/util.hpp"

namespace onyx::db::pg {

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

// NOTE: libqp 源代碼會檢查傳入 nullptr
// ref: postgres/src/interfaces/libpq/fe-connect.c
bool Connection::is_connected() const noexcept { return PQstatus(conn_.get()) == CONNECTION_OK; }

std::string_view Connection::db_name() const noexcept { return PQdb(conn_.get()); }
std::string_view Connection::user() const noexcept { return PQuser(conn_.get()); }
std::string_view Connection::pass() const noexcept { return PQpass(conn_.get()); }
std::string_view Connection::host() const noexcept { return PQhost(conn_.get()); }
std::string_view Connection::port() const noexcept { return PQport(conn_.get()); }

// ----------------------------------------------------------------------------
// Private Methods
// ----------------------------------------------------------------------------
Result<ResultPtr> Connection::execute_internal(const char* sql) const noexcept {
  if (!is_connected()) {
    return onyx::fail(DbErrc::ConnectionFail);
  }

  ResultPtr res(PQexec(conn_.get(), sql));
  ExecStatusType status = PQresultStatus(res.get());
  if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
    char* msg = PQerrorMessage(conn_.get());
    return onyx::fail(DbErrc::QueryFail, msg);
  }

  return res;
}

// ----------------------------------------------------------------------------
// Connection Management
// ----------------------------------------------------------------------------

Result<void> Connection::connect(const char* conninfo) noexcept {
  conn_.reset(PQconnectdb(conninfo));

  if (!is_connected()) {
    char* msg = PQerrorMessage(conn_.get());
    return onyx::fail(DbErrc::ConnectionFail, msg);
  }

  return {};
}

void Connection::close() noexcept { conn_.reset(); }

// ----------------------------------------------------------------------------
// Simple Query
// ----------------------------------------------------------------------------

Result<int> Connection::execute(const char* sql) const noexcept {
  ResultPtr res = TRY(execute_internal(sql));

  char* affected_rows_str = PQcmdTuples(res.get());
  if (!affected_rows_str || affected_rows_str[0] == '\0') {
    return 0;
  }
  int affected_rows = TRY(parse_value<int>(affected_rows_str));
  return affected_rows;
}

Result<int> Connection::execute(std::string& sql) const noexcept {
  ResultPtr res = TRY(execute_internal(sql.c_str()));

  char* affected_rows_str = PQcmdTuples(res.get());
  if (!affected_rows_str || affected_rows_str[0] == '\0') {
    return 0;
  }
  int affected_rows = TRY(parse_value<int>(affected_rows_str));
  return affected_rows;
}

Result<ResultSet> Connection::query(const char* sql) const noexcept {
  ResultPtr res = TRY(execute_internal(sql));
  return ResultSet(std::move(res));
}

Result<ResultSet> Connection::query(std::string& sql) const noexcept {
  ResultPtr res = TRY(execute_internal(sql.c_str()));
  return ResultSet(std::move(res));
}

// ----------------------------------------------------------------------------
// Transaction
// ----------------------------------------------------------------------------

Result<Transaction> Connection::begin_transaction() noexcept {
  if (in_transaction_) {
    return onyx::fail(DbErrc::QueryFail, "Already in trancation");
  }

  CHECK(execute("BEGIN"));
  in_transaction_ = true;
  return Transaction(this);
}

Result<void> Connection::commit() noexcept {
  if (!in_transaction_) {
    return onyx::fail(DbErrc::QueryFail, "Not in transaction");
  }

  CHECK(execute("COMMIT"));

  in_transaction_ = false;
  return {};
}

Result<void> Connection::rollback() noexcept {
  if (!in_transaction_) {
    return onyx::fail(DbErrc::QueryFail, "Not in transaction");
  }
  CHECK(execute("ROLLBACK"));

  in_transaction_ = false;
  return {};
}

// ----------------------------------------------------------------------------
// Manual Prepared Statement
// ----------------------------------------------------------------------------

Result<std::string> Connection::prepare(const char* sql) const noexcept {
  if (!is_connected()) {
    return onyx::fail(DbErrc::ConnectionFail);
  }

  // 生成唯一的 Statement 名稱
  std::string stmt_name =
      "stmt_" + std::to_string(stmt_counter_.fetch_add(1, std::memory_order_relaxed));

  ResultPtr res(PQprepare(conn_.get(), stmt_name.c_str(), sql, 0, nullptr));
  ExecStatusType status = PQresultStatus(res.get());
  if (status != PGRES_COMMAND_OK) {
    char* msg = PQerrorMessage(conn_.get());
    return onyx::fail(DbErrc::QueryFail, msg);
  }
  return stmt_name;
}

}  // namespace onyx::db::pg
