#include "tx/io/buf_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <expected>

#include "tx/error.hpp"

namespace tx::io {

// ----------------------------------------------------------------------------
// Private Methods
// ----------------------------------------------------------------------------

Result<size_t> BufReader::fill_buffer() noexcept {
  if (buffered() > 0) {  // 裡面還有就先移到前面
    std::memmove(buffer_.data(), buffer_.data() + pos_, buffered());
    valid_ = buffered();
    pos_ = 0;
  } else {
    pos_ = 0;
    valid_ = 0;
  }

  auto result =
      file_.read(std::span(buffer_.data() + valid_, buffer_.size() - valid_));

  size_t n = TRY(result);

  valid_ += n;
  return n;
}

// ----------------------------------------------------------------------------
// Factory Methods
// ----------------------------------------------------------------------------

Result<BufReader> BufReader::from_file(File file, size_t capacity) noexcept {
  if (capacity == 0) {
    return tx::fail(std::errc::invalid_argument, "Buffer capacity must be > 0");
  }
  return BufReader(std::move(file), capacity);
}

// ----------------------------------------------------------------------------
// RAII
// ----------------------------------------------------------------------------

BufReader::BufReader(File file, size_t capacity) noexcept
    : file_(std::move(file)), buffer_(capacity) {}

// ----------------------------------------------------------------------------
// Buffered Operations
// ----------------------------------------------------------------------------

[[nodiscard]] Result<size_t> BufReader::read(
    std::span<std::byte> dest) noexcept {
  if (dest.empty()) {
    return 0;
  }

  size_t total_read = 0;
  if (buffered() > 0) {  // 先讀 buffer
    size_t to_copy = std::min(dest.size(), buffered());
    std::memcpy(dest.data(), buffer_.data() + pos_, to_copy);
    pos_ += to_copy;
    total_read += to_copy;
    dest = dest.subspan(to_copy);  // 從 to_copy 到最後
  }

  // dest 比 buffer 還大就直接讀取文件
  if (!dest.empty() && dest.size() >= buffer_.size()) {
    size_t n = TRY(file_.read(dest));
    total_read += n;
    return total_read;
  }

  if (!dest.empty()) {
    size_t filled = TRY(fill_buffer());

    if (filled == 0) {
      return total_read;  // EOF
    }

    size_t to_copy = std::min(dest.size(), buffered());
    std::memcpy(dest.data(), buffer_.data() + pos_, to_copy);
    pos_ += to_copy;
    total_read += to_copy;
  }

  return total_read;
}

Result<> BufReader::read_exact(std::span<std::byte> dest) noexcept {
  size_t remaining = dest.size();

  while (remaining > 0) {
    size_t n = TRY(read(dest));

    if (n == 0) {  // EOF
      return tx::fail(std::errc::no_message_available, "Unexpected EOF");
    }

    dest = dest.subspan(n);
    remaining -= n;
  }

  return {};
}

Result<std::vector<std::byte>> BufReader::read_until(
    std::byte delimiter) noexcept {
  std::vector<std::byte> result;

  while (true) {
    if (buffered() == 0) {
      size_t n = TRY(fill_buffer());
      if (n == 0) {  // EOF
        break;
      }
    }

    // 在的當前 buffer 中找尋 delimiter
    std::byte* begin = buffer_.data() + pos_;
    std::byte* end = buffer_.data() + valid_;
    auto found = std::find(begin, end, delimiter);

    if (found != end) {  // 找到
      size_t len = static_cast<size_t>(found - begin) + 1;
      result.insert(result.end(), begin, found + 1);
      pos_ += len;
      return result;
    } else {
      result.insert(result.end(), begin, end);
      pos_ = valid_;
    }
  }
  return result;
}

Result<std::vector<std::byte>> BufReader::read_to_end() noexcept {
  std::vector<std::byte> result;

  if (buffered() > 0) {
    result.insert(result.end(), buffer_.data() + pos_, buffer_.data() + valid_);
    pos_ = valid_;
  }

  while (true) {
    size_t n = TRY(fill_buffer());
    if (n == 0) {  // EOF
      break;
    }

    result.insert(result.end(), buffer_.data() + pos_, buffer_.data() + valid_);
    pos_ = valid_;
  }

  return result;
}

Result<std::string> BufReader::read_line() noexcept {
  std::vector<std::byte> bytes = TRY(read_until(std::byte('\n')));

  if (bytes.empty()) {
    return tx::fail(std::errc::no_message_available);
  }

  std::string result;
  result.reserve(bytes.size());

  for (auto b : bytes) {
    char c = static_cast<char>(b);
    // 排除 '\n' 與 '\r'
    if (c != '\n' && c != '\r') {
      result.push_back(c);
    }
  }

  return result;
}

Result<size_t> BufReader::read_line_into(std::string& buf) noexcept {
  std::vector<std::byte> bytes = TRY(read_until(std::byte('\n')));

  if (bytes.empty()) {
    return 0;
  }

  buf.reserve(buf.size() + bytes.size());

  size_t start_size = buf.size();
  for (auto b : bytes) {
    char c = static_cast<char>(b);
    buf.push_back(c);
  }

  return buf.size() - start_size;
}

Result<std::vector<std::string>> BufReader::read_lines() noexcept {
  std::vector<std::string> lines;

  while (true) {
    auto line = read_line();

    if (!line) {
      if (line.error() == std::errc::no_message_available) {  // EOF
        break;
      }

      return std::unexpected(line.error());
    }
    lines.push_back(std::move(*line));
  }

  return lines;
}

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

Result<bool> BufReader::is_eof() noexcept {
  if (buffered() > 0) {
    return false;
  }

  // 嘗試填充 buffer
  size_t filled = TRY(fill_buffer());

  return filled == 0;
}

}  // namespace tx::io
