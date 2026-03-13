#ifndef ONYX_UTIL_HPP
#define ONYX_UTIL_HPP

#include <charconv>
#include <type_traits>

#include "onyx/error.hpp"
#include "onyx/trait.hpp"

namespace onyx {

template <typename T>
[[nodiscard]] Result<T> parse_value(std::string_view str) {
  if constexpr (std::is_same_v<T, std::string_view>) {
    return str;
  } else if constexpr (std::is_same_v<T, std::string>) {
    return std::string(str);
  } else if constexpr (std::is_integral_v<T>) {
    T result{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
    if (ec != std::errc{}) {
      return onyx::fail(std::errc::invalid_argument, "Integral parsing failed");
    }

    return result;
  } else if constexpr (std::is_floating_point_v<T>) {
    T result{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
    if (ec != std::errc{}) {
      return onyx::fail(std::errc::invalid_argument, "Float parsing failed");
    }
    return result;
  } else {
    static_assert(always_false_v<T>, "Unsupported type mapping for PostgreSQL column");
  }
}

}  // namespace onyx

#endif
