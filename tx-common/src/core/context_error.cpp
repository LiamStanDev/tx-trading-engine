#include "tx/core/context_error.hpp"

#include <fmt/format.h>

namespace tx::core {

ContextError ContextError::from_errno() noexcept {
  return ContextError(std::make_error_code(static_cast<std::errc>(errno)));
}

ContextError ContextError::from_errno(std::string context) noexcept {
  return ContextError(std::error_code(errno, std::generic_category()),
                      std::move(context));
}

ContextError ContextError::from_errc(std::errc ec) noexcept {
  return ContextError(std::make_error_code(ec));
}

ContextError ContextError::from_errc(std::errc ec,
                                     std::string context) noexcept {
  return ContextError(std::make_error_code(ec), std::move(context));
}

ContextError ContextError::from_code(std::error_code code) noexcept {
  return ContextError(code);
}

ContextError ContextError::from_code(std::error_code code,
                                     std::string context) noexcept {
  return ContextError(code, std::move(context));
}

std::string ContextError::message() const noexcept {
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
struct fmt::formatter<tx::core::ContextError> : fmt::formatter<std::string> {
  auto format(const tx::core::ContextError& err, format_context& ctx) const {
    return fmt::formatter<std::string>::format(err.message(), ctx);
  }
};
