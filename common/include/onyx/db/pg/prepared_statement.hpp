#ifndef ONYX_DB_PG_PREPARED_STATEMENT_HPP
#define ONYX_DB_PG_PREPARED_STATEMENT_HPP

#include <libpq-fe.h>

#include <string>
#include <vector>

#include "onyx/db/pg/result_set.hpp"
#include "onyx/error.hpp"

namespace onyx::db::pg {

class PreparedStatement {
 private:
  PGconn* conn_;
  std::string stmt_name_;  ///< Prepared Statement 名稱

 public:
  // ----------------------------------------------------------------------------
  // Constructor
  // ----------------------------------------------------------------------------

  PreparedStatement(PGconn* conn, std::string_view stmt_name) noexcept
      : conn_(conn), stmt_name_(stmt_name) {}

  template <typename... Args>
  Result<int> execute(Args&&... args) noexcept;

  template <typename... Args>
  Result<ResultSet> query(Args&&... args) noexcept;

 private:
  Result<ResultPtr> execute_internal(const std::vector<const char*>& param_values) const noexcept;
};

}  // namespace onyx::db::pg

#endif
