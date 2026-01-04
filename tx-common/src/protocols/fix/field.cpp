#include "tx/protocols/fix/field.hpp"

#include <charconv>

namespace tx::protocols::fix {

std::optional<int> FieldView::to_int() const {
  int result = 0;
  auto [ptr, ec] =
      std::from_chars(value.data(), value.data() + value.size(), result);

  if (ec != std::errc{}) {
    return std::nullopt;
  }

  if (ptr != value.cend()) {
    return std::nullopt;
  }

  return result;
}

std::optional<double> FieldView::to_double() const {
  double result = 0;
  auto [ptr, ec] =
      std::from_chars(value.data(), value.data() + value.size(), result);

  if (ec != std::errc{}) {
    return std::nullopt;
  }

  if (ptr != value.cend()) {
    return std::nullopt;
  }

  return result;
}

}  // namespace tx::protocols::fix
