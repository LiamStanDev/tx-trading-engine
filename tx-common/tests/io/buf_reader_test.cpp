#include "tx/io/buf_reader.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>

#include "test_util.hpp"

namespace tx::io::test {

// ----------------------------------------------------------------------------
// Factory & RAII
// ----------------------------------------------------------------------------

TEST(BufReaderTest, FromFile_ValidFile_DefaultCapacity) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("test data"));

  auto file = File::open(temp.path(), O_RDONLY);
  ASSERT_TRUE(file);

  auto reader = BufReader::from_file(std::move(*file));
  ASSERT_TRUE(reader);
  EXPECT_EQ(reader->capacity(), BufReader::kDefaultCapacity);
  EXPECT_TRUE(reader->underlying_file().is_open());
}

}  // namespace tx::io::test
