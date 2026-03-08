#include "onyx/net/taifex/codec.hpp"

#include <chrono>
#include <cstdint>

#include "onyx/core/type.hpp"

namespace onyx::net::taifex {

void encode_bcd(uint8_t* dst, size_t dst_len, uint64_t val) noexcept {
  // 數值 1234 → 0x12 0x34
  // 數值 18500 → 0x01 0x85 0x00（3 bytes）
  for (int i = static_cast<int>(dst_len) - 1; i >= 0; --i) {
    uint8_t low = static_cast<uint8_t>(val % 10);
    val /= 10;
    uint8_t high = static_cast<uint8_t>(val % 10);
    val /= 10;
    dst[i] = static_cast<uint8_t>(high << 4) | low;  // 4 是因為 16 進制一個位置為 2^4
  }
}

uint64_t unpack_bcd(std::span<const uint8_t> bcd_bytes) noexcept {
  uint64_t res = 0;
  for (uint8_t bcd_byte : bcd_bytes) {
    uint8_t high = (bcd_byte >> 4) & 0x0F;
    uint8_t low = bcd_byte & 0x0F;
    res = res * 100 + high * 10 + low;
  }

  return res;
}

Price unpack_bcd_price(std::span<const uint8_t> bcd_bytes, int decimal_places, char sign) noexcept {
  int64_t raw_value;
  if (sign == '-') [[unlikely]] {
    raw_value = -static_cast<int64_t>(unpack_bcd(bcd_bytes));
  } else {
    raw_value = static_cast<int64_t>(unpack_bcd(bcd_bytes));
  }

  assert(decimal_places <= 4 && "decimal_places exceeds Price precision");

  static constexpr int64_t scale_table[] = {10000, 1000, 100, 10, 1};
  int64_t scale_factor = scale_table[decimal_places];

  return Price::from_raw(static_cast<int64_t>(raw_value) * scale_factor);
}

std::chrono::nanoseconds unpack_bcd_time(std::span<const uint8_t> bcd_bytes) noexcept {
  uint64_t val = unpack_bcd(bcd_bytes);

  // HH:mm:ss:mmm:uuu
  uint64_t us = val % 1'000;                      // 微秒
  uint64_t ms = (val / 1'000) % 1'000;            // 毫秒
  uint64_t ss = (val / 1'000'000) % 100;          // 秒
  uint64_t mm = (val / 100'000'000) % 100;        // 分
  uint64_t hh = (val / 10'000'000'000ULL) % 100;  // 時

  return std::chrono::hours(hh) + std::chrono::minutes(mm) + std::chrono::seconds(ss) +
         std::chrono::milliseconds(ms) + std::chrono::microseconds(us);
}

}  // namespace onyx::net::taifex
