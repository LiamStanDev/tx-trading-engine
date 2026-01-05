#ifndef TX_TRADING_ENGINE_PROTOCOLS_FIELD_HPP
#define TX_TRADING_ENGINE_PROTOCOLS_FIELD_HPP

#include <optional>
#include <string_view>
#include <vector>

#include "tx/protocols/fix/error.hpp"

namespace tx::protocols::fix {

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
  [[nodiscard]] static Result<MessageView> parse(
      std::string_view buffer) noexcept;
};

}  // namespace tx::protocols::fix

#endif
