#include "tx/core/price.hpp"

#include <fmt/core.h>

namespace tx::core {
std::string Price::to_string() const {
  return fmt::format("Price({})", to_points());
}
}  // namespace tx::core
