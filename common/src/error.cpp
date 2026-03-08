#include "onyx/error.hpp"

#include "execinfo.h"

namespace onyx {

const ErrorContext& ErrorHandle::context() const noexcept {
  return onyx::ErrorRegistry::get(*this);
}

std::error_code ErrorHandle::ec() const noexcept { return context().ec; }

const char* ErrorHandle::message() const noexcept { return context().message; }

std::error_code make_error_code(int ev) noexcept { return {ev, std::generic_category()}; }

ErrorHandle ErrorRegistry::capture_origin(std::error_code ec, const char* msg,
                                          std::source_location loc) noexcept {
  uint32_t current_id = head_++;
  auto& ctx = buffer_[current_id % SIZE];
  ctx.ec = ec;
  ctx.message = msg;
  ctx.location = loc;
  // NOTE: 我這邊轉換是安全的，因為返回值不會超過參數的 10
  ctx.frame_count = static_cast<uint8_t>(::backtrace(ctx.stackframe, 8));
  return ErrorHandle{current_id};
}

ErrorHandle ErrorRegistry::capture_origin_thin(std::error_code ec, const char* msg,
                                               std::source_location loc) noexcept {
  uint32_t current_id = head_++;
  auto& ctx = buffer_[current_id % SIZE];
  ctx.ec = ec;
  ctx.message = msg;
  ctx.location = loc;
  ctx.frame_count = 0;  // 不捕捉堆疊
  return ErrorHandle{current_id};
}

}  // namespace onyx
