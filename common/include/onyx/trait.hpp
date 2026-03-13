#ifndef ONYX_TRAIT_HPP
#define ONYX_TRAIT_HPP

#include <optional>
#include <type_traits>
namespace onyx {

template <typename T>
inline constexpr bool always_false_v = false;

template <typename>
struct is_optional : std::false_type {};

template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <typename U>
inline constexpr bool is_optional_v = is_optional<U>::value;

}  // namespace onyx

#endif
