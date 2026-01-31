#include "tx/io/buf_reader.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>

#include "test_util.hpp"

namespace tx::io::test {

// ----------------------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------------------

class BufReaderTest : public ::testing::Test {
 protected:
  // 建立臨時檔案並返回 BufReader
  BufReader create_reader(std::string_view content,
                          size_t capacity = BufReader::kDefaultCapacity) {
    TempFile temp;
    EXPECT_TRUE(temp.is_valid());
    EXPECT_TRUE(temp.write_content(content));

    auto file = File::open(temp.path(), O_RDONLY);
    EXPECT_TRUE(file);

    temp_files_.push_back(std::move(temp));
    return BufReader::with_capacity(std::move(*file), capacity);
  }

  std::vector<TempFile> temp_files_;
};

// ----------------------------------------------------------------------------
// Factory & RAII
// ----------------------------------------------------------------------------

TEST_F(BufReaderTest, FromFile_DefaultCapacity) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("test"));

  auto file = File::open(temp.path(), O_RDONLY);
  ASSERT_TRUE(file);

  auto reader = BufReader::from_file(std::move(*file));
  EXPECT_EQ(reader.capacity(), BufReader::kDefaultCapacity);
}

// ----------------------------------------------------------------------------
// Basic Operations
// ----------------------------------------------------------------------------

TEST_F(BufReaderTest, Read_BasicData) {
  auto reader = create_reader("Hello, World!");

  std::vector<std::byte> buf(5);
  auto result = reader.read(buf);

  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 5);

  std::string data(reinterpret_cast<char*>(buf.data()), 5);
  EXPECT_EQ(data, "Hello");
}

TEST_F(BufReaderTest, ReadExact_Success) {
  auto reader = create_reader("1234567890");

  std::vector<std::byte> buf(10);
  auto result = reader.read_exact(buf);

  ASSERT_TRUE(result);
  std::string data(reinterpret_cast<char*>(buf.data()), 10);
  EXPECT_EQ(data, "1234567890");
}

TEST_F(BufReaderTest, ReadExact_EOF_ReturnsError) {
  auto reader = create_reader("short");

  std::vector<std::byte> buf(100);
  auto result = reader.read_exact(buf);

  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), std::errc::no_message_available);
}

// ----------------------------------------------------------------------------
// Read Until & To End
// ----------------------------------------------------------------------------

TEST_F(BufReaderTest, ReadUntil_IncludesDelimiter) {
  auto reader = create_reader("Hello\nWorld");

  auto result = reader.read_until(std::byte('\n'));
  ASSERT_TRUE(result);

  std::string data(reinterpret_cast<char*>(result->data()), result->size());
  EXPECT_EQ(data, "Hello\n");  // 必須包含 \n
}

TEST_F(BufReaderTest, ReadUntil_EOF_ReturnsPartialData) {
  auto reader = create_reader("no newline");

  auto result = reader.read_until(std::byte('\n'));
  ASSERT_TRUE(result);  // EOF 不是錯誤

  std::string data(reinterpret_cast<char*>(result->data()), result->size());
  EXPECT_EQ(data, "no newline");
}

TEST_F(BufReaderTest, ReadToEnd_Success) {
  auto reader = create_reader("1234567890");

  // 先讀 3 bytes
  std::vector<std::byte> buf(3);
  ASSERT_TRUE(reader.read(buf));

  // 讀取剩餘
  auto result = reader.read_to_end();
  ASSERT_TRUE(result);

  std::string remaining(reinterpret_cast<char*>(result->data()),
                        result->size());
  EXPECT_EQ(remaining, "4567890");
}

// ----------------------------------------------------------------------------
// Line Operations
// ----------------------------------------------------------------------------

TEST_F(BufReaderTest, ReadLine_FiltersNewlines) {
  auto reader = create_reader("line1\nline2\r\nline3\n");

  auto line1 = reader.read_line();
  ASSERT_TRUE(line1);
  EXPECT_EQ(*line1, "line1");

  auto line2 = reader.read_line();
  ASSERT_TRUE(line2);
  EXPECT_EQ(*line2, "line2");  // \r\n 都被過濾

  auto line3 = reader.read_line();
  ASSERT_TRUE(line3);
  EXPECT_EQ(*line3, "line3");
}

TEST_F(BufReaderTest, ReadLineInto_KeepsNewlines) {
  auto reader = create_reader("test\r\n");

  std::string buf;
  auto result = reader.read_line_into(buf);

  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 6);
  EXPECT_EQ(buf, "test\r\n");  // 保留 \r 和 \n
}

TEST_F(BufReaderTest, ReadLines_Success) {
  auto reader = create_reader("line1\n\nline3\n");

  auto lines = reader.read_lines();
  ASSERT_TRUE(lines);

  ASSERT_EQ(lines->size(), 3);
  EXPECT_EQ((*lines)[0], "line1");
  EXPECT_EQ((*lines)[1], "");  // 保留空行
  EXPECT_EQ((*lines)[2], "line3");
}

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

TEST_F(BufReaderTest, IsEOF_WithData_ReturnsFalse) {
  auto reader = create_reader("data");

  auto eof = reader.is_eof();
  ASSERT_TRUE(eof);
  EXPECT_FALSE(*eof);
}

TEST_F(BufReaderTest, IsEOF_EmptyFile_ReturnsTrue) {
  auto reader = create_reader("");

  auto eof = reader.is_eof();
  ASSERT_TRUE(eof);
  EXPECT_TRUE(*eof);
}
}  // namespace tx::io::test
