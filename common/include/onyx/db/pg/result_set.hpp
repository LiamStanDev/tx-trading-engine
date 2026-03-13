#ifndef ONYX_DB_PG_RESULT_SET_HPP
#define ONYX_DB_PG_RESULT_SET_HPP

#include <libpq-fe.h>

#include <memory>
#include <optional>
#include <string_view>

#include "onyx/error.hpp"
#include "onyx/trait.hpp"
#include "onyx/util.hpp"

namespace onyx::db::pg {

class Row {
 private:
  PGresult* res_;
  int row_idx_;

 public:
  Row(PGresult* res, int row_idx) noexcept : res_(res), row_idx_(row_idx) {};

  template <typename T>
  Result<T> get(int col_idx) const noexcept;

  template <typename T>
  Result<T> get(const char* col_name) const noexcept;
};

struct ResultDeleter {
  void operator()(PGresult* res) const noexcept {
    if (res) {
      PQclear(res);
    }
  }
};

using ResultPtr = std::unique_ptr<PGresult, ResultDeleter>;

class ResultSet {
 private:
 public:
 private:
  ResultPtr res_;
  int num_rows_;

 public:
  // ----------------------------------------------------------------------------
  // Constructor
  // ----------------------------------------------------------------------------

  explicit ResultSet(ResultPtr res) noexcept {
    num_rows_ = PQntuples(res.get());
    res_ = std::move(res);
  }

  // ----------------------------------------------------------------------------
  // Iterator
  // ----------------------------------------------------------------------------

  class Iterator {
   private:
    PGresult* res_;
    int current_row_;

   public:
    Iterator(PGresult* res, int row) noexcept : res_(res), current_row_(row) {}

    Row operator*() const noexcept { return Row(res_, current_row_); }

    Iterator& operator++() noexcept {
      ++current_row_;
      return *this;
    }

    bool operator!=(const Iterator& other) const { return current_row_ != other.current_row_; }
  };

  Iterator begin() const { return Iterator(res_.get(), 0); }
  Iterator end() const { return Iterator(res_.get(), num_rows_); }

  // ----------------------------------------------------------------------------
  // Query
  // ----------------------------------------------------------------------------

  int size() const noexcept { return num_rows_; }
  bool empty() const noexcept { return num_rows_ == 0; }
};

template <typename T>
Result<T> Row::get(int col_idx) const noexcept {
  if (col_idx < 0 || col_idx >= PQnfields(res_)) {
    return onyx::fail(std::errc::invalid_argument, "Column index out of range");
  }

  const bool is_null = PQgetisnull(res_, row_idx_, col_idx);

  if constexpr (is_optional_v<T>) {
    if (is_null) {
      return std::nullopt;
    }
    using InnerType = typename T::value_type;  // optional 的內部類型

    const char* value = PQgetvalue(res_, row_idx_, col_idx);
    const int len = PQgetlength(res_, row_idx_, col_idx);
    std::string_view sv(value, static_cast<size_t>(len));
    InnerType inner_result = TRY(parse_value<InnerType>(sv));
    return std::make_optional(std::move(inner_result));
  } else {
    if (is_null) {
      return T{};
    }

    const char* value = PQgetvalue(res_, row_idx_, col_idx);
    const int len = PQgetlength(res_, row_idx_, col_idx);
    std::string_view sv(value, static_cast<size_t>(len));
    return parse_value<T>(sv);
  }
}

template <typename T>
Result<T> Row::get(const char* col_name) const noexcept {
  int col_idx = PQfnumber(res_, col_name);

  if (col_idx == -1) {
    return onyx::fail(std::errc::invalid_argument, "Column name not found");
  }

  return get<T>(col_idx);
}

}  // namespace onyx::db::pg

#endif
