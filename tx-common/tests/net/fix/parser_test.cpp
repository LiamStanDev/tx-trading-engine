// #include "tx/protocols/fix/parser.hpp"
//
// #include <gtest/gtest.h>
//
// #include "tx/protocols/fix/constains.hpp"
//
// namespace tx::protocols::fix::test {
//
// TEST(FIXParserTest, ParseValidMessage) {
//   const std::string msg =
//       "8=FIX.4.2\x01"
//       "9=40\x01"
//       "35=D\x01"
//       "49=SENDER\x01"
//       "56=TARGET\x01"
//       "34=1\x01"
//       "10=150\x01";
//
//   auto result = Parser::parse(msg);
//
//   ASSERT_TRUE(result.is_ok());
//   auto& message = *result;
//
//   EXPECT_EQ(message.begin_string, "FIX.4.2");
//   EXPECT_EQ(message.body_length, 40);
//   EXPECT_EQ(message.msg_type, "D");
//
//   auto sender = message.find_field(tags::SenderCompId);
//   ASSERT_TRUE(sender.has_value());
//   ASSERT_EQ(sender->value, "SENDER");
// }
//
// TEST(FIXParserTest, InvalidChecksumShouldFail) {
//   const std::string msg =
//       "8=FIX.4.2\x01"
//       "9=40\x01"
//       "35=D\x01"
//       "49=SENDER\x01"
//       "56=TARGET\x01"
//       "10=999\x01";  // ← 故意錯誤的 checksum
//
//   auto result = Parser::parse(msg);
//   ASSERT_TRUE(result.is_err());
//   EXPECT_TRUE(result.error().is(FixErrc::InvalidCheckSum));
// }
//
// TEST(FIXParserTest, InvalidTagFormatShouldFail) {
//   const std::string msg =
//       "8=FIX.4.2\x01"
//       "abc=40\x01";  // ← 非數字 Tag
//
//   auto result = Parser::parse(msg);
//   ASSERT_TRUE(result.is_err());
// }
//
// }  // namespace tx::protocols::fix::test
