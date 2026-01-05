#include "tx/protocols/fix/parser.hpp"

#include <charconv>
#include <cstddef>
#include <optional>

#include "tx/protocols/fix/constains.hpp"
#include "tx/protocols/fix/error.hpp"

namespace tx::protocols::fix {

std::optional<int> FieldView::to_int() const {
  int result = 0;
  auto [ptr, ec] =
      std::from_chars(value.data(), value.data() + value.size(), result);

  if (ec != std::errc{}) {
    return std::nullopt;
  }

  if (ptr != value.cend()) {
    return std::nullopt;
  }

  return result;
}

std::optional<double> FieldView::to_double() const {
  double result = 0;
  auto [ptr, ec] =
      std::from_chars(value.data(), value.data() + value.size(), result);

  if (ec != std::errc{}) {
    return std::nullopt;
  }

  if (ptr != value.cend()) {
    return std::nullopt;
  }

  return result;
}

std::optional<FieldView> MessageView::find_field(int tag) noexcept {
  for (auto field : fields) {
    if (field.tag == tag) {
      return field;
    }
  }
  return std::nullopt;
}

Result<MessageView> Parser::parse(std::string_view buffer) noexcept {
  if (buffer.empty()) {
    return Err(FixError::from(FixErrc::EmptyMessage));
  }

  MessageView msg;
  std::string_view remaining = buffer;

  // 解析 BeginString: Tag 8
  auto [f8, r8] = parse_field(buffer);
  if (f8.tag != tags::BeginString) {
    return Err(FixError::from(FixErrc::MissingBeginString));
  }
  msg.begin_string = f8.value;
  remaining = r8;

  // 解析 BodyLength: Tag 9
  auto [f9, r9] = parse_field(remaining);
  if (f9.tag != tags::BodyLength) {
    return Err(FixError::from(FixErrc::MissingBodyLength));
  }

  auto body_length_opt = f9.to_int();
  if (!body_length_opt.has_value()) {
    return Err(FixError::from(FixErrc::InvalidFormat));
  }
  msg.body_length = *(body_length_opt);
  remaining = r9;

  // 解析 MsgType: Tag 35
  auto [f35, r35] = parse_field(remaining);
  if (f35.tag != tags::MsgType) {
    return Err(FixError::from(FixErrc::InvalidFormat));
  }
  msg.msg_type = f35.value;
  remaining = r35;

  while (!remaining.empty()) {
    auto [field, ret] = parse_field(remaining);
    if (field.tag == -1) break;

    if (field.tag == tags::Checksum) {
      msg.checksum = field.to_int().value_or(0);

      // 計算 checksum 範圍: 從消息頭部一直到 checksum tag ('1=') 之前
      size_t checksum_offset =
          static_cast<size_t>(remaining.data() - buffer.data());
      int checksum = calculate_checksum(buffer.substr(0, checksum_offset));
      if (checksum != msg.checksum) {
        return Err(FixError::from(FixErrc::InvalidCheckSum));
      }
      return Ok(msg);
    }
    msg.fields.push_back(field);
    remaining = ret;
  }

  return Err(FixError::from(FixErrc::MissingChecksum));
}

std::pair<FieldView, std::string_view> Parser::parse_field(
    std::string_view buffer) noexcept {
  // 格式 Tag=Value<SOH>
  size_t eq_pos = buffer.find('=');
  if (eq_pos == std::string_view::npos) {
    return {FieldView{-1, {}}, {}};
  }

  // 解析 Tag
  int tag = 0;
  auto [ptr, ec] = std::from_chars(buffer.data(), buffer.data() + eq_pos, tag);
  if (ec != std::errc{} || ptr != buffer.data() + eq_pos) {
    return {FieldView{-1, {}}, {}};
  }

  size_t soh_pos = buffer.find(SOH, eq_pos + 1);

  if (soh_pos == std::string_view::npos) {
    return {FieldView{-1, {}}, {}};
  }

  // 解析 Value
  std::string_view value = buffer.substr(eq_pos + 1, soh_pos - eq_pos - 1);
  return {FieldView{tag, value}, buffer.substr(soh_pos + 1)};
}

int Parser::calculate_checksum(std::string_view buffer) noexcept {
  int sum = 0;
  for (char c : buffer) {
    sum += c;
  }

  return sum % 256;
}

}  // namespace tx::protocols::fix
