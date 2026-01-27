#ifndef TX_TRADING_ENGINE_PROTOCOLS_FIELD_HPP
#define TX_TRADING_ENGINE_PROTOCOLS_FIELD_HPP

#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "tx/utils.hpp"

namespace tx::protocols::fix {

enum class fix_parse_errc : uint8_t {
  empty_message = 1,
  invalid_format,
  missing_begin_string,
  missing_body_length,
  missing_message_type,
  missing_checksum,
  invalid_checksum
};

const std::error_category& fix_parse_category() noexcept;

const std::error_code make_error_code(fix_parse_errc ec) noexcept;

struct FieldView {
  int tag;
  std::string_view value;

  [[nodiscard]] std::optional<int> to_int() const;
  [[nodiscard]] std::optional<double> to_double() const;
};

struct MessageView {
  std::string_view begin_string;  ///< Tag 8
  int body_length{};              ///< Tag 9
  std::string_view msg_type;      ///< Tag 35
  std::vector<FieldView> fields;  ///< Body 中的欄位(不包含 Header 與 Trailer)
  int checksum{};                 ///< Tag 10

  [[nodiscard]] std::optional<FieldView> find_field(int tag) noexcept;
};

class Parser {
 private:
  [[nodiscard]] static std::pair<FieldView, std::string_view> parse_field(
      std::string_view buffer) noexcept;

  [[nodiscard]] static int calculate_checksum(std::string_view buffer) noexcept;

 public:
  [[nodiscard]] static expected<MessageView> parse(
      std::string_view buffer) noexcept;
};

}  // namespace tx::protocols::fix

template <>
struct std::is_error_code_enum<tx::protocols::fix::fix_parse_errc>
    : std::true_type {};

#endif
