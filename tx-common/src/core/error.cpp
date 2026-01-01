#include "tx/core/error.hpp"

#include <fmt/format.h>

#include <system_error>

namespace tx::core {

Error Error::from_errno() noexcept {
  return Error(std::make_error_code(static_cast<std::errc>(errno)));
}

Error Error::from_errno(std::string context) noexcept {
  return Error(std::error_code(errno, std::generic_category()),
               std::move(context));
}

Error Error::from_errc(std::errc ec) noexcept {
  return Error(std::make_error_code(ec));
}

Error Error::from_errc(std::errc ec, std::string context) noexcept {
  return Error(std::make_error_code(ec), std::move(context));
}

Error Error::from_code(std::error_code code) noexcept { return Error(code); }

Error Error::from_code(std::error_code code, std::string context) noexcept {
  return Error(code, std::move(context));
}

std::string Error::message() const noexcept {
  bool is_generic = code_.category() == std::generic_category();

  std::string header =
      fmt::format("[{}:{}]: {}", is_generic ? "SYS" : code_.category().name(),
                  code_.value(), code_.message());
  if (context_.empty()) {
    return header;
  }

  return fmt::format("{}\n └─▶ context: {}", header, context_);
}

}  // namespace tx::core

/// @brief 支持 fmt
template <>
struct fmt::formatter<tx::core::Error> : fmt::formatter<std::string> {
  auto format(const tx::core::Error& err, format_context& ctx) const {
    return fmt::formatter<std::string>::format(err.message(), ctx);
  }
};
