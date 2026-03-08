#ifndef ONYX_NET_TAIFEX_CODEC_HPP
#define ONYX_NET_TAIFEX_CODEC_HPP

#include <stdint.h>

#include <chrono>
#include <span>

#include "onyx/core/type.hpp"

namespace onyx::net::taifex {

using namespace onyx::core;

/// @brief 將整數編碼成 PACK BCD
///
/// @param dst 目標陣列
/// @param dst_len 長度
/// @param val 要編碼的整數
void encode_bcd(uint8_t* dst, size_t dst_len, uint64_t val) noexcept;

/// @brief 將 PACK BCD 格式轉回數值
///
/// @note PACK BCD 格式會用一個 byte 存放兩個十進值整數，前四位為第一個後四位為第二個
uint64_t unpack_bcd(std::span<const uint8_t> bcd_bytes) noexcept;

/// @brief 將 PACK BCD 轉換為 Price (需要知道小數位數)
///
/// @param bcd_bytes 待解析位元陣列
/// @param sign 價格正負號，價格正負號 '0' 正號, '-' 負號(複式商品專用)
/// @param decimal_places 小數點位數
Price unpack_bcd_price(std::span<const uint8_t> bcd_bytes, int decimal_places,
                       char sign = '0') noexcept;

/// @brief 將 PACK BCD 轉換為 chrono 納秒時間戳
std::chrono::nanoseconds unpack_bcd_time(std::span<const uint8_t> bcd_bytes) noexcept;

}  // namespace onyx::net::taifex

#endif
