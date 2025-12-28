#include <fmt/core.h>

#include <tx/core/price.hpp>

namespace tx::core {
std::string Price::to_string() const {
  return fmt::format("Price({})", to_points());  // ✅
}
}  // namespace tx::core
