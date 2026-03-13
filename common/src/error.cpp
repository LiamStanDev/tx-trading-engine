#include "onyx/error.hpp"

#include <algorithm>
#include <cstring>

#include "execinfo.h"

namespace onyx {

const ErrorContext& ErrorHandle::context() const noexcept {
  return onyx::ErrorRegistry::get(*this);
}

std::error_code ErrorHandle::ec() const noexcept { return context().ec; }

std::string_view ErrorHandle::message() const noexcept { return context().message; }

std::error_code make_error_code(int ev) noexcept { return {ev, std::generic_category()}; }

ErrorHandle ErrorRegistry::capture_origin(std::error_code ec, std::string_view msg,
                                          std::source_location loc) noexcept {
  uint32_t id = head_++;
  uint32_t slot = id % SIZE;
  auto& ctx_slot = ctx_buffer_[slot];
  auto& msg_slot = msg_buffer_[slot];

  size_t msg_len = std::min(msg.length(), MSG_MAX_LEN);
  if (msg_len > 0) {
    std::memcpy(msg_slot.data(), msg.data(), msg_len);
  }
  msg_slot[msg_len] = '\0';
  ctx_slot.message = msg_slot.data();

  ctx_slot.ec = ec;
  ctx_slot.location = loc;
  ctx_slot.frame_count =
      static_cast<uint8_t>(::backtrace(ctx_slot.stackframe, ErrorContext::MAX_FRAME_COUNT));
  return ErrorHandle{id};
}

ErrorHandle ErrorRegistry::capture_origin_thin(std::error_code ec, std::string_view msg,
                                               std::source_location loc) noexcept {
  uint32_t id = head_++;
  size_t slot = id % SIZE;
  auto& ctx_slot = ctx_buffer_[slot];
  auto& msg_slot = msg_buffer_[slot];

  size_t msg_len = std::min(msg.length(), MSG_MAX_LEN);
  if (msg_len > 0) {
    std::memcpy(msg_slot.data(), msg.data(), msg_len);
  }
  msg_slot[msg_len] = '\0';
  ctx_slot.message = msg_slot.data();

  ctx_slot.ec = ec;
  ctx_slot.location = loc;
  ctx_slot.frame_count = 0;  // 不捕捉堆疊
  return ErrorHandle{id};
}

}  // namespace onyx
