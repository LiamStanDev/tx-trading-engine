#include "tx/io/file.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>

#include <span>
#include <thread>
#include <vector>

#include "./test_util.hpp"
#include "tx/error.hpp"

namespace tx::io::test {

// ----------------------------------------------------------------------------
// Factory Methods & RAII
// ----------------------------------------------------------------------------

TEST(FileTest, OpenExistingFile) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("text context"));

  auto file = File::open(temp.path(), O_RDONLY);
  ASSERT_TRUE(file);
  EXPECT_TRUE(file->is_open());
  EXPECT_GE(file->fd(), 0);
  EXPECT_EQ(file->path(), temp.path());
}

TEST(FileTest, OpenNonExistentFile_ReturnsENOENT) {
  auto file = File::open("/nonexistent/file.txt", O_RDONLY);
  EXPECT_FALSE(file);
  EXPECT_EQ(file.error().value(), ENOENT);
}

TEST(FileTest, OpenWithInvalidPermissions_ReturnsEACCES) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("data"));

  ::chmod(temp.path().c_str(), 0000);

  auto result = File::open(temp.path(), O_RDONLY);
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().value(), EACCES);

  ::chmod(temp.path().c_str(), 0644);
}

TEST(FileTest, OpenDirectory_ReturnsEISDIR) {
  auto result = File::open("/tmp", O_WRONLY);

  // 嘗試以寫入模式開啟目錄應該失敗
  if (!result) {
    EXPECT_EQ(result.error().value(), EISDIR);
  }
}

TEST(FileTest, CreateWithOCREAT) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  // 先刪除檔案
  ::unlink(temp.path().c_str());

  auto file = File::open(temp.path(), O_RDWR | O_CREAT, 0644);
  ASSERT_TRUE(file);
  EXPECT_TRUE(file->is_open());
}

TEST(FileTest, CreateTemp_GeneratesValidFile) {
  std::string template_path = "/tmp/test-XXXXXX";
  auto file = File::create_temp(template_path);

  ASSERT_TRUE(file);
  EXPECT_TRUE(file->is_open());
  EXPECT_NE(file->path(), "/tmp/test-XXXXXX");
  EXPECT_TRUE(file->path().starts_with("/tmp/test-"));

  // 清理
  ::unlink(file->path().c_str());
}

TEST(FileTest, MoveAssignment_ClosesOldFd) {
  TempFile temp1;
  TempFile temp2;

  ASSERT_TRUE(temp1.is_valid());
  ASSERT_TRUE(temp2.is_valid());

  auto file1 = File::open(temp1.path(), O_RDONLY).value();
  auto file2 = File::open(temp2.path(), O_RDONLY).value();

  int fd1 = file1.fd();
  int fd2 = file2.fd();

  file1 = std::move(file2);

  EXPECT_EQ(file1.fd(), fd2);
  EXPECT_FALSE(file2.is_open());

  // 透過 fcntl 取得該失效 fd 應該要返回 -1 並且設定 errno 為 EBADF
  EXPECT_EQ(::fcntl(fd1, F_GETFD), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(FileTest, Destructor_ClosesFd) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  int fd;
  {
    auto file = File::open(temp.path(), O_RDONLY).value();
    fd = file.fd();
    EXPECT_GE(fd, 0);
  }  // file 析構

  // 檢查 fd 是否關閉
  EXPECT_EQ(::fcntl(fd, F_GETFD), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(FileTest, Release_ReturnsFdWithoutClosing) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  int fd;
  {
    auto file = File::open(temp.path(), O_RDONLY).value();
    fd = file.release();
    EXPECT_FALSE(file.is_open());
  }

  // fd 應該仍然有效
  EXPECT_GE(::fcntl(fd, F_GETFD), 0);
  ::close(fd);
}

// ----------------------------------------------------------------------------
// Basic I/O
// ----------------------------------------------------------------------------

TEST(FileTest, ReadEntireFile) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  std::string content = "Hello, World!";
  ASSERT_TRUE(temp.write_content(content));

  auto file = File::open(temp.path(), O_RDONLY).value();
  std::vector<std::byte> buffer(content.size());

  auto result = file.read(buffer);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, content.size());

  std::string read_content(reinterpret_cast<const char*>(buffer.data()),
                           buffer.size());
  EXPECT_EQ(read_content, content);
}

TEST(FileTest, ReadPartialContent) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("1234567890"));

  auto file = File::open(temp.path(), O_RDONLY).value();
  std::vector<std::byte> buffer(5);

  auto result = file.read(buffer);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 5);

  std::string read_content(reinterpret_cast<const char*>(buffer.data()), 5);
  EXPECT_EQ(read_content, "12345");
}

TEST(FileTest, ReadZeroBytes) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("data"));

  auto file = File::open(temp.path(), O_RDONLY).value();
  std::vector<std::byte> buffer(0);

  auto result = file.read(buffer);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 0);
}

TEST(FileTest, ReadBeyondEOF_ReturnsZero) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("short"));

  auto file = File::open(temp.path(), O_RDONLY).value();
  std::vector<std::byte> buffer(100);

  auto result1 = file.read(buffer);
  ASSERT_TRUE(result1);
  EXPECT_EQ(*result1, 5);

  auto result2 = file.read(buffer);
  ASSERT_TRUE(result2);
  EXPECT_EQ(*result2, 0);  // EOF
}

TEST(FileTest, ReadFromClosedFile_ReturnsError) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_RDONLY).value();
  file.close();

  std::vector<std::byte> buffer(10);
  auto result = file.read(buffer);

  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), std::errc::bad_file_descriptor);
}

TEST(FileTest, WriteData) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_WRONLY | O_TRUNC).value();

  std::string data = "test data";
  auto bytes = std::as_bytes(std::span(data));

  auto result = file.write(bytes);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, data.size());

  file.close();

  auto content = temp.read_content();
  ASSERT_TRUE(content);
  EXPECT_EQ(*content, data);
}

TEST(FileTest, WriteMultipleTimes) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_WRONLY | O_TRUNC).value();

  std::string data1 = "Hello";
  std::string data2 = " World";

  auto _1 = file.write(std::as_bytes(std::span(data1)));
  auto _2 = file.write(std::as_bytes(std::span(data2)));

  file.close();

  auto content = temp.read_content();
  ASSERT_TRUE(content);
  EXPECT_EQ(*content, "Hello World");
}

TEST(FileTest, ReadWriteCycle) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_RDWR | O_TRUNC).value();

  std::string original = "test content";
  auto _1 = file.write(std::as_bytes(std::span(original)));

  ASSERT_TRUE(file.rewind());

  std::vector<std::byte> buffer(original.size());
  auto _2 = file.read(buffer);

  std::string read_back(reinterpret_cast<const char*>(buffer.data()),
                        buffer.size());
  EXPECT_EQ(read_back, original);
}

TEST(FileTest, LargeFileIO) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_RDWR | O_TRUNC).value();

  auto data = random_bytes(1024 * 1024);  // 1 MB
  auto write_result = file.write(data);
  ASSERT_TRUE(write_result);
  EXPECT_EQ(*write_result, data.size());

  ASSERT_TRUE(file.rewind());
  std::vector<std::byte> read_buffer(data.size());
  auto read_result = file.read(read_buffer);
  ASSERT_TRUE(read_result);
  EXPECT_EQ(*read_result, data.size());
  EXPECT_EQ(read_buffer, data);
}

// ----------------------------------------------------------------------------
// Positional I/O
// ----------------------------------------------------------------------------

TEST(FileTest, Pread_DoesNotChangePosition) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  std::vector<std::byte> buffer(3);
  auto result = file.pread(buffer, 5);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 3);

  auto pos = file.tell();
  ASSERT_TRUE(pos);
  EXPECT_EQ(*pos, 0);
}

TEST(FileTest, Pwrite_DoesNotChangePosition) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDWR).value();

  std::string data = "XXX";
  auto result = file.pwrite(std::as_bytes(std::span(data)), 3);
  ASSERT_TRUE(result);

  auto pos = file.tell();
  ASSERT_TRUE(pos);
  EXPECT_EQ(*pos, 0);

  file.close();
  EXPECT_EQ(temp.read_content().value(), "012XXX6789");
}

TEST(FileTest, PreadWithNegativeOffset_ReturnsError) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_RDONLY).value();

  std::vector<std::byte> buffer(10);
  auto result = file.pread(buffer, -1);

  EXPECT_EQ(result.error(), std::errc::invalid_argument);
  EXPECT_STREQ(ErrorRegistry::get_last_error().message, "Invalid offset");
}

TEST(FileTest, PwriteWithNegativeOffset_ReturnsError) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_WRONLY).value();

  std::string data = "test";
  auto result = file.pwrite(std::as_bytes(std::span(data)), -1);

  EXPECT_EQ(result.error(), std::errc::invalid_argument);
  EXPECT_STREQ(ErrorRegistry::get_last_error().message, "Invalid offset");
}

TEST(FileTest, ConcurrentPreadPwrite_ThreadSafe) {
  TempFile temp;
  ASSERT_TRUE(temp.write_content(std::string(1000, 'A')));

  auto file = File::open(temp.path(), O_RDWR).value();

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&, i]() {
      std::string data(10, 'A' + i);
      auto write_result = file.pwrite(std::as_bytes(std::span(data)), i * 10);
      ASSERT_TRUE(write_result);

      std::vector<std::byte> buffer(10);
      auto read_result = file.pread(buffer, i * 10);
      ASSERT_TRUE(read_result);
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  SUCCEED();
}

// ----------------------------------------------------------------------------
// Seek
// ----------------------------------------------------------------------------

TEST(FileTest, SeekSet_FromBeginning) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  auto result = file.seek(5, File::Whence::Begin);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 5);
}

TEST(FileTest, SeekCur_FromCurrent) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  auto _ = file.seek(3, File::Whence::Begin);
  auto result = file.seek(2, File::Whence::Current);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 5);
}

TEST(FileTest, SeekEnd_FromEnd) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  auto result = file.seek(-3, File::Whence::End);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 7);
}

TEST(FileTest, Tell_ReturnsCurrentPosition) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  auto _ = file.seek(7);
  auto pos = file.tell();
  ASSERT_TRUE(pos);
  EXPECT_EQ(*pos, 7);
}

TEST(FileTest, Rewind_ResetsToBeginning) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("0123456789"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  auto _ = file.seek(5);
  auto result = file.rewind();
  ASSERT_TRUE(result);

  auto pos = file.tell();
  ASSERT_TRUE(pos);
  EXPECT_EQ(*pos, 0);
}

TEST(FileTest, SeekBeyondEOF_Allowed) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("short"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  auto result = file.seek(1000);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, 1000);
}

// ============================================================================
// Sync & Size
// ============================================================================

TEST(FileTest, Sync_Succeeds) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_RDWR).value();

  std::string data = "test";
  auto _ = file.write(std::as_bytes(std::span(data)));

  auto result = file.sync();
  ASSERT_TRUE(result);
}

TEST(FileTest, Datasync_Succeeds) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());

  auto file = File::open(temp.path(), O_RDWR).value();

  std::string data = "test";
  auto _ = file.write(std::as_bytes(std::span(data)));

  auto result = file.datasync();
  ASSERT_TRUE(result);
}

TEST(FileTest, Size_ReturnsCorrectSize) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  std::string content = "1234567890";
  ASSERT_TRUE(temp.write_content(content));

  auto file = File::open(temp.path(), O_RDONLY).value();
  auto size = file.size();

  ASSERT_TRUE(size);
  EXPECT_EQ(*size, content.size());
}

TEST(FileTest, Resize_ExpandsFile) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("short"));

  auto file = File::open(temp.path(), O_RDWR).value();
  auto result = file.resize(100);
  ASSERT_TRUE(result);

  auto size = file.size();
  ASSERT_TRUE(size);
  EXPECT_EQ(*size, 100);
}

TEST(FileTest, Resize_ShrinksFile) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("long content here"));

  auto file = File::open(temp.path(), O_RDWR).value();
  auto result = file.resize(5);
  ASSERT_TRUE(result);

  file.close();

  auto content = temp.read_content();
  ASSERT_TRUE(content);
  EXPECT_EQ(*content, "long ");
}

// ----------------------------------------------------------------------------
// Advise
// ----------------------------------------------------------------------------

TEST(FileTest, Advise_AllTypes) {
  TempFile temp;
  ASSERT_TRUE(temp.is_valid());
  ASSERT_TRUE(temp.write_content("test data"));

  auto file = File::open(temp.path(), O_RDONLY).value();

  EXPECT_TRUE(file.advise(File::Advise::Normal));
  EXPECT_TRUE(file.advise(File::Advise::Sequential));
  EXPECT_TRUE(file.advise(File::Advise::Random));
  EXPECT_TRUE(file.advise(File::Advise::WillNeed));
  EXPECT_TRUE(file.advise(File::Advise::DontNeed));
}

}  // namespace tx::io::test
