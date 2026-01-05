#include "tx/protocols/fix/message_builder.hpp"

#include <fmt/compile.h>
#include <fmt/format.h>

#include <charconv>
#include <iterator>

#include "tx/protocols/fix/constains.hpp"

namespace tx::protocols::fix {

Result<std::string> MessageBuilder::build() const noexcept {
  if (msg_type_.empty()) {
    return Err(FixError::from(FixErrc::MissingMsgType));
  }
  if (sender_comp_id_.empty()) {
    return Err(FixError::from(FixErrc::MissingSender));
  }
  if (target_comp_id_.empty()) {
    return Err(FixError::from(FixErrc::MissingTarget));
  }
  if (msg_seq_num_ <= 0) {
    return Err(FixError::from(FixErrc::InvalidSeqSum));  // 或新增 InvalidSeqNum
  }
  if (sending_time_.empty()) {
    return Err(FixError::from(FixErrc::MissingSendingTime));
  }

  // 策略：先構建 body，再計算 body_length，最後組裝完整訊息
  // 這樣避免了預留空間和後續的 erase 操作

  std::string body;
  body.reserve(estimate_size());

  // Body 欄位（從 Tag 35 開始）
  // Tag 35: MsgType
  append_tag_str(body, tags::MsgType, msg_type_);

  // Sender/Target
  append_tag_str(body, tags::SenderCompId, sender_comp_id_);
  append_tag_str(body, tags::TargetCompId, target_comp_id_);

  // MsgSeqNum
  append_tag_int(body, tags::MsgSeqSum, msg_seq_num_);

  // SendingTime
  append_tag_str(body, tags::SendingTime, sending_time_);

  // 其他 Body 欄位
  for (const auto& field : fields_) {
    append_tag_str(body, field.tag, field.value);
  }

  // 計算 BodyLength
  size_t body_length = body.size();

  if (body_length > constraints::kMaxBodyLength) {
    return Err(FixError::from(FixErrc::BodyLengthExceeded));
  }

  // 現在構建完整訊息
  std::string result;
  result.reserve(estimate_size());

  // Tag 8: BeginString
  append_tag_str(result, tags::BeginString, begin_string_);

  // Tag 9: Body Length（直接構建，無需預留空間）
  append_tag_int(result, tags::BodyLength, static_cast<int>(body_length));

  // 追加 Body
  result.append(body);

  // Tag 10: Checksum
  int checksum = calculate_checksum(result);
  append_checksum(result, checksum);

  return Ok(std::move(result));
}

void MessageBuilder::append_tag_str(std::string& str, int tag,
                                    std::string_view value) noexcept {
  fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}={}\x01"), tag, value);
}

void MessageBuilder::append_tag_int(std::string& str, int tag,
                                    int value) noexcept {
  fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}={}\x01"), tag, value);
}

/// @brief 追加 checksum tag @details 獨立於 append_tag_int 是因爲 FIX
/// 格式規定要補齊到三位
void MessageBuilder::append_checksum(std::string& str, int checksum) noexcept {
  fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}={:03}\x01"),
                 tags::Checksum, checksum);
}

int MessageBuilder::calculate_checksum(std::string_view msg) noexcept {
  int sum = 0;
  for (char c : msg) {
    sum += static_cast<int>(c);
  }
  return sum % constraints::kChecksumModulo;
}

/// @brief 估算最終訊息大小
/// @details 用於 std::string::reserve() 減少記憶體重分配
size_t MessageBuilder::estimate_size() const noexcept {
  size_t size = 0;
  // Tag 8: "8=FIX.4.2\x01" = 11
  size += 2 + begin_string_.size() + 1;

  // Tag 9: "9=12345\x01" = 8 (預留最大值)
  size += constraints::kBodyLengthFieldReserve;

  // Tag 35: "35=D\x01"
  size += 2 + msg_type_.size() + 1;

  // Tag 49
  size += 2 + sender_comp_id_.size() + 1;

  // Tag 56
  size += 2 + target_comp_id_.size() + 1;

  // Tag 34: "34=123456\x01" (假設最多 6 位數)
  size += 2 + 6 + 1;

  // Tag 52
  size += 2 + sending_time_.size() + 1;

  // Body fields
  for (const auto& field : fields_) {
    // tag 通常是 1-4 位數, =, field 大小, + SOH
    size += 4 + 1 + field.value.size() + 1;
  }

  // Tag 10: "10=123\x01" = 7
  size += 7;

  return size;
}

}  // namespace tx::protocols::fix
