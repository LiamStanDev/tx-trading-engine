#include <fmt/format.h>

#include <tx/network/address_error.hpp>

namespace tx::network {

constexpr const char* to_string(AddressErrorCode code) noexcept {
  using enum AddressErrorCode;
  switch (code) {
    case InvalidFormat:
      return "InvalidFormat";
    case InvalidPort:
      return "InvalidPort";
    case ParseFailure:
      return "ParseFailure";
    case UnsupportedFamily:
      return "UnsupportedFamily";
  }

  return "Unknowed";
}

const char* AddressError::code_name() const noexcept {
  return to_string(code_);
}

std::string AddressError::message() const noexcept {
  return fmt::format("[{}]: {}", code_name(), detail_);
}

}  // namespace tx::network
