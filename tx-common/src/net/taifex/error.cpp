#include "tx/net/taifex/error.hpp"

#include <arpa/inet.h>

#include <system_error>

namespace tx::net::taifex {

std::error_category& parse_category() noexcept {
  static struct : public std::error_category {
    const char* name() const noexcept override { return "tx.protocols.taifex"; }
    std::string message(int ev) const override {
      switch (static_cast<parse_errc>(ev)) {
        case parse_errc::invalid_esc_code:
          return "Invalid EscCode (expected 0x1B)";
        case parse_errc::invalid_packet_length:
          return "Invalid packet length";
        case parse_errc::invalid_msg_count:
          return "Invalid message count";
        case parse_errc::buffer_too_small:
          return "Buffer too small";
        case parse_errc::invalid_msg_kind:
          return "Invalid message kind (expected 'R')";
        case parse_errc::invalid_msg_type:
          return "Invalid message type";
        case parse_errc::invalid_msg_length:
          return "Invalid message length";
        default:
          return "Unknown taifex parse error";
      }
    }
  } instance;

  return instance;
}

std::error_code make_error_code(parse_errc ec) noexcept {
  return std::error_code(static_cast<int>(ec), parse_category());
}

}  // namespace tx::net::taifex
