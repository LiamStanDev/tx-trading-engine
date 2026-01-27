#include "tx/error.hpp"

namespace tx {

std::error_code make_error_code(int ev) noexcept {
  return {ev, std::generic_category()};
}

void ErrorRegistry::capture_origin(std::error_code ec, const char* msg,
                                   std::source_location loc) noexcept {
  last_info = {.ec = ec, .location = loc, .message = msg, .is_active = true};
}

}  // namespace tx
