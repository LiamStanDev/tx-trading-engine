#include "onyx/core/type.hpp"

#include <fmt/core.h>

namespace onyx::core {
std::string Price::to_string() const { return fmt::format("Price({})", to_double()); }
}  // namespace onyx::core
