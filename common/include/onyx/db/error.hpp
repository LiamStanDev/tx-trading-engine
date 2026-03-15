
#ifndef ONYX_DB_PG_ERROR_HPP
#define ONYX_DB_PG_ERROR_HPP

#include <cstdint>
#include <system_error>

namespace onyx::db {

enum class DbErrc : uint8_t {
  ConnectionFail = 1,  ///< 資料庫連線失敗
  QueryFail,           ///< 執行 Query 失敗
  // TypeParseErr,        ///< 類型解析錯誤
};

class DbErrorCategory : public std::error_category {
 public:
  const char* name() const noexcept override { return "onyx::db"; }

  std::string message(int ev) const override {
    using enum DbErrc;
    switch (static_cast<DbErrc>(ev)) {
      case ConnectionFail:
        return "Database connection lost or failed";
      case QueryFail:
        return "SQL query error or fail";
    }

    return "Unknow Error";
  }
};

inline std::error_code make_error_code(DbErrc ec) noexcept {
  static const DbErrorCategory instance;
  return {static_cast<int>(ec), instance};
}

}  // namespace onyx::db

namespace std {
template <>
struct is_error_code_enum<onyx::db::DbErrc> : true_type {};
}  // namespace std

#endif
