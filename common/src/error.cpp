#include "onyx/error.hpp"

#include <algorithm>
#include <cstring>
#include <source_location>

#include "execinfo.h"

namespace onyx {

const ErrorContext& ErrorToken::context() const noexcept { return onyx::ErrorRegistry::get(); }

std::error_code ErrorToken::ec() const noexcept { return context().ec; }

std::string ErrorToken::message() const noexcept { return context().message; }

std::error_code make_error_code(int ev) noexcept { return {ev, std::generic_category()}; }

ErrorToken ErrorRegistry::capture_fail(std::error_code ec, std::string_view msg,
                                       std::source_location loc) noexcept {
  ctx_.ec = ec;
  ctx_.location = loc;
  ctx_.frame_count =
      static_cast<uint8_t>(::backtrace(ctx_.stackframe, ErrorContext::MAX_FRAME_COUNT));
  ctx_.message = msg;
  return ErrorToken{};
}

ErrorToken ErrorRegistry::capture_flow(std::error_code ec, std::source_location loc) noexcept {
  ctx_.ec = ec;
  ctx_.location = loc;
  ctx_.frame_count = 0;  // 不捕捉堆疊
  ctx_.message.clear();

  return ErrorToken{};
}

}  // namespace onyx
