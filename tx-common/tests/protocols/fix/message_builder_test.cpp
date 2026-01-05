#include "tx/protocols/fix/message_builder.hpp"

#include <gtest/gtest.h>

#include <charconv>

#include "tx/protocols/fix/constains.hpp"
#include "tx/protocols/fix/parser.hpp"

namespace tx::protocols::fix::test {

// ==========================
// Helper 函數
// ==========================

/// @brief 驗證 FIX 訊息的基本結構
/// @details 檢查是否包含必要的 SOH 分隔符
void AssertValidFixStructure(std::string_view msg) {
  ASSERT_FALSE(msg.empty());
  ASSERT_EQ(msg[msg.size() - 1], SOH) << "訊息應以 SOH 結尾";

  // 檢查是否包含關鍵 tags
  EXPECT_NE(msg.find("8="), std::string::npos) << "缺少 BeginString (Tag 8)";
  EXPECT_NE(msg.find("9="), std::string::npos) << "缺少 BodyLength (Tag 9)";
  EXPECT_NE(msg.find("35="), std::string::npos) << "缺少 MsgType (Tag 35)";
  EXPECT_NE(msg.find("10="), std::string::npos) << "缺少 Checksum (Tag 10)";
}

/// @brief 計算預期的 checksum
/// @details 用於驗証 builder 的計算是否正確
int calculate_expected_checksum(std::string_view body) {
  int sum = 0;
  for (char c : body) {
    sum += static_cast<int>(c);
  }

  return sum % 256;
}

int extract_checksum(std::string_view msg) {
  auto pos = msg.find("10=");
  if (pos == std::string::npos) {
    return -1;
  }

  pos += 3;
  int checksum = 0;
  std::from_chars(msg.data() + pos, msg.data() + 3, checksum);

  return checksum;
}

// ==========================
// 基礎功能測試
// ==========================

TEST(MessageBuilderTest, SetHeaderFields) {
  MessageBuilder builder("D");

  auto& result1 = builder.set_sender("SENDER123");
  EXPECT_EQ(&result1, &builder) << "set_sender 應返回 *this";

  auto& result2 = builder.set_target("TARGET456");
  EXPECT_EQ(&result2, &builder) << "set_target 應返回 *this";

  auto& result3 = builder.set_msg_seq_num(42);
  EXPECT_EQ(&result3, &builder) << "set_msg_seq_num 應返回 *this";

  auto& result4 = builder.set_sending_time("20260105-10:30:00");
  EXPECT_EQ(&result4, &builder) << "set_sending_time 應返回 *this";
}

TEST(MessageBuilderTest, AddBodyFields) {
  MessageBuilder builder("D");

  auto& result1 = builder.add_field(11, "ORDER123");
  EXPECT_EQ(&result1, &builder);

  auto& result2 = builder.add_field(55, "AAPL");
  EXPECT_EQ(&result2, &builder);

  auto& result3 = builder.add_field(54, 1);  // Side: Buy
  EXPECT_EQ(&result3, &builder);

  auto& result4 = builder.add_field(44, 150.75, 2);  // Price
  EXPECT_EQ(&result4, &builder);
}

TEST(MessageBuilderTest, BuildSimpleMessage) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();

  ASSERT_TRUE(result.is_ok()) << "應該成功構建訊息";

  const std::string& msg = *result;
  AssertValidFixStructure(msg);

  // 驗證關鍵內容
  EXPECT_NE(msg.find("8=FIX.4.2"), std::string::npos);
  EXPECT_NE(msg.find("35=D"), std::string::npos);
  EXPECT_NE(msg.find("49=SENDER"), std::string::npos);
  EXPECT_NE(msg.find("56=TARGET"), std::string::npos);
  EXPECT_NE(msg.find("34=1"), std::string::npos);
  EXPECT_NE(msg.find("52=20260105-10:30:00"), std::string::npos);
}

TEST(MessageBuilderTest, BuildWithMultipleFields) {
  MessageBuilder builder("D");
  builder.set_sender("TRADER01")
      .set_target("EXCHANGE")
      .set_msg_seq_num(123)
      .set_sending_time("20260105-14:25:30")
      .add_field(11, "ORD001")    // ClOrdID
      .add_field(55, "AAPL")      // Symbol
      .add_field(54, 1)           // Side: Buy
      .add_field(38, 100)         // OrderQty
      .add_field(40, 2)           // OrdType: Limit
      .add_field(44, 150.50, 2);  // Price

  auto result = builder.build();

  ASSERT_TRUE(result.is_ok());

  const std::string& msg = *result;
  AssertValidFixStructure(msg);

  // 驗證 body fields
  EXPECT_NE(msg.find("11=ORD001"), std::string::npos);
  EXPECT_NE(msg.find("55=AAPL"), std::string::npos);
  EXPECT_NE(msg.find("54=1"), std::string::npos);
  EXPECT_NE(msg.find("38=100"), std::string::npos);
  EXPECT_NE(msg.find("40=2"), std::string::npos);
  EXPECT_NE(msg.find("44=150.50"), std::string::npos);
}

TEST(MessageBuilderTest, MultipleBuildCallsProduceSameResult) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00")
      .add_field(11, "ORDER123");

  // 第一次 build
  auto result1 = builder.build();
  ASSERT_TRUE(result1.is_ok());

  // 第二次 build（應該產生相同結果）
  auto result2 = builder.build();
  ASSERT_TRUE(result2.is_ok());

  // 兩次結果應該完全相同
  EXPECT_EQ(*result1, *result2) << "多次 build() 應產生相同結果";
}

// ==========================
// 錯誤測試
// ==========================

TEST(MessageBuilderTest, BuildWithoutSender) {
  MessageBuilder builder("D");
  // 故意不設定 sender
  builder.set_target("TARGET").set_msg_seq_num(1).set_sending_time(
      "20260105-10:30:00");

  auto result = builder.build();

  ASSERT_TRUE(result.is_err());
  EXPECT_TRUE(result.error().is(FixErrc::MissingSender));
}

TEST(MessageBuilderTest, BuildWithoutTarget) {
  MessageBuilder builder("D");
  builder
      .set_sender("SENDER")
      // 故意不設定 target
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();

  ASSERT_TRUE(result.is_err());
  EXPECT_TRUE(result.error().is(FixErrc::MissingTarget));
}

TEST(MessageBuilderTest, BuildWithInvalidSeqNum) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(0)  // ← 無效：必須 > 0
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();

  ASSERT_TRUE(result.is_err());
}

TEST(MessageBuilderTest, BuildWithNegativeSeqNum) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(-1)  // ← 無效
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();

  ASSERT_TRUE(result.is_err());
}

TEST(MessageBuilderTest, BuildWithoutSendingTime) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER").set_target("TARGET").set_msg_seq_num(1);
  // 故意不設定 sending_time

  auto result = builder.build();

  ASSERT_TRUE(result.is_err());
  EXPECT_TRUE(result.error().is(FixErrc::MissingSendingTime));
}

TEST(MessageBuilderTest, BodyLengthExceeded) {
  MessageBuilder builder("D");
  builder.set_sender("S").set_target("T").set_msg_seq_num(1).set_sending_time(
      "T");

  // 構造一個超大的欄位（接近 100KB）
  std::string large_value(constraints::kMaxBodyLength, 'X');
  builder.add_field(5000, large_value);

  auto result = builder.build();

  // 應該返回 BodyLengthExceeded 錯誤
  ASSERT_TRUE(result.is_err());
  EXPECT_TRUE(result.error().is(FixErrc::BodyLengthExceeded));
}

// ==========================
// 邊界測試
// ==========================

TEST(MessageBuilderTest, MinimalValidMessage) {
  MessageBuilder builder("D");
  builder.set_sender("S").set_target("T").set_msg_seq_num(1).set_sending_time(
      "T");

  auto result = builder.build();

  ASSERT_TRUE(result.is_ok()) << "最小有效訊息應該能成功構建";
  AssertValidFixStructure(*result);
}

TEST(MessageBuilderTest, LargeSeqNum) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(999999)  // 6 位數
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();

  ASSERT_TRUE(result.is_ok());
  EXPECT_NE(result->find("34=999999"), std::string::npos);
}

TEST(MessageBuilderTest, ManyFields) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00");

  // 新增 50 個 body fields
  for (int i = 0; i < 50; ++i) {
    builder.add_field(5000 + i, std::to_string(i));
  }

  auto result = builder.build();

  ASSERT_TRUE(result.is_ok());
  AssertValidFixStructure(*result);
}

/// =======================
/// 正確性測試
/// =======================

TEST(MessageBuilderTest, CorrectFieldOrder) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00")
      .add_field(11, "ORDER123");

  auto result = builder.build();
  ASSERT_TRUE(result.is_ok());

  const std::string& msg = *result;

  // 驗證 FIX 標準的欄位順序
  size_t pos_tag8 = msg.find("8=");
  size_t pos_tag9 = msg.find("9=");
  size_t pos_tag35 = msg.find("35=");
  size_t pos_tag49 = msg.find("49=");
  size_t pos_tag56 = msg.find("56=");
  size_t pos_tag34 = msg.find("34=");
  size_t pos_tag52 = msg.find("52=");
  size_t pos_tag10 = msg.find("10=");

  // 標準順序：8 → 9 → 35 → 49 → 56 → 34 → 52 → (body) → 10
  EXPECT_LT(pos_tag8, pos_tag9);
  EXPECT_LT(pos_tag9, pos_tag35);
  EXPECT_LT(pos_tag35, pos_tag49);
  EXPECT_LT(pos_tag49, pos_tag56);
  EXPECT_LT(pos_tag56, pos_tag34);
  EXPECT_LT(pos_tag34, pos_tag52);
  EXPECT_GT(pos_tag10, pos_tag52);  // checksum 在最後
}

TEST(MessageBuilderTest, CorrectBodyLengthCalculation) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();
  ASSERT_TRUE(result.is_ok());

  const std::string& msg = *result;

  // 手動提取 BodyLength 值
  size_t pos = msg.find("9=");
  ASSERT_NE(pos, std::string::npos);
  pos += 2;  // 跳過 "9="

  int declared_length = 0;
  while (pos < msg.size() && msg[pos] >= '0' && msg[pos] <= '9') {
    declared_length = declared_length * 10 + (msg[pos] - '0');
    pos++;
  }

  // 計算實際 body length
  size_t body_start = msg.find(SOH, msg.find("9=")) + 1;
  size_t checksum_pos = msg.find("10=");
  int actual_length = static_cast<int>(checksum_pos - body_start);

  EXPECT_EQ(declared_length, actual_length)
      << "聲明的 BodyLength 應等於實際長度";
}

TEST(MessageBuilderTest, CorrectChecksumCalculation) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00");

  auto result = builder.build();
  ASSERT_TRUE(result.is_ok());

  const std::string& msg = *result;

  // 提取 checksum field 之前的所有內容
  size_t checksum_pos = msg.find("10=");
  ASSERT_NE(checksum_pos, std::string::npos);

  std::string msg_without_checksum = msg.substr(0, checksum_pos);

  // 計算預期的 checksum
  int expected_checksum = calculate_expected_checksum(msg_without_checksum);

  // 提取實際的 checksum
  int actual_checksum = extract_checksum(msg);

  EXPECT_EQ(actual_checksum, expected_checksum) << "Checksum 計算不正確";
}

TEST(MessageBuilderTest, SOHDelimiters) {
  MessageBuilder builder("D");
  builder.set_sender("SENDER")
      .set_target("TARGET")
      .set_msg_seq_num(1)
      .set_sending_time("20260105-10:30:00")
      .add_field(11, "ORDER123");

  auto result = builder.build();
  ASSERT_TRUE(result.is_ok());

  const std::string& msg = *result;

  // 計算 SOH 的數量
  int soh_count = 0;
  for (char c : msg) {
    if (c == SOH) soh_count++;
  }

  // 應該至少有：Tag 8, 9, 35, 49, 56, 34, 52, 11, 10 = 9 個 SOH
  EXPECT_GE(soh_count, 9) << "SOH 分隔符數量不足";
}

/// ================================
/// 與 Parser 整合測試
/// ================================

TEST(MessageBuilderTest, BuildAndParseRoundTrip) {
  // 構建訊息
  MessageBuilder builder("D");
  builder.set_sender("TRADER01")
      .set_target("EXCHANGE")
      .set_msg_seq_num(42)
      .set_sending_time("20260105-14:25:30")
      .add_field(11, "ORD001")
      .add_field(55, "AAPL")
      .add_field(54, 1);

  auto build_result = builder.build();
  ASSERT_TRUE(build_result.is_ok());

  const std::string& msg = *build_result;

  // 解析訊息
  auto parse_result = Parser::parse(msg);
  ASSERT_TRUE(parse_result.is_ok()) << "Parser 應該能解析 Builder 構建的訊息";

  auto& parsed = *parse_result;

  // 驗證解析結果
  EXPECT_EQ(parsed.begin_string, "FIX.4.2");
  EXPECT_EQ(parsed.msg_type, "D");

  auto sender = parsed.find_field(tags::SenderCompId);
  ASSERT_TRUE(sender.has_value());
  EXPECT_EQ(sender->value, "TRADER01");

  auto target = parsed.find_field(tags::TargetCompId);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->value, "EXCHANGE");

  auto clordid = parsed.find_field(tags::ClOrdID);
  ASSERT_TRUE(clordid.has_value());
  EXPECT_EQ(clordid->value, "ORD001");

  auto symbol = parsed.find_field(tags::Symbol);
  ASSERT_TRUE(symbol.has_value());
  EXPECT_EQ(symbol->value, "AAPL");
}

}  // namespace tx::protocols::fix::test
