#ifndef TX_TRADING_ENGINE_NET_TAIFEX_ERROR_HPP
#define TX_TRADING_ENGINE_NET_TAIFEX_ERROR_HPP

#include <arpa/inet.h>

#include <cstdint>
#include <system_error>

namespace tx::net::taifex {

/// @brief 錯誤碼
enum class parse_errc : uint8_t {
  buffer_too_small = 1,
  invalid_esc_code,
  invalid_msg_count,
  invalid_packet_length,
  invalid_msg_kind,
  invalid_msg_type,
  invalid_msg_length,
};

std::error_category& parse_category() noexcept;

std::error_code make_error_code(parse_errc ec) noexcept;

}  // namespace tx::net::taifex

template <>
struct std::is_error_code_enum<tx::net::taifex::parse_errc> : std::true_type {};

#endif
