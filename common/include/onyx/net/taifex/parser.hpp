#ifndef ONYX_NET_TAIFEX_PARSER_HPP
#define ONYX_NET_TAIFEX_PARSER_HPP

#include <stdint.h>

#include <optional>
#include <span>

#include "onyx/net/taifex/message.hpp"

namespace onyx::net::taifex {

using namespace onyx::core;

/// @brief 解析 I010 商品基本資料訊息
///
/// @param buffer 必須為 I010 訊息格式，內部不會檢查
/// @return 成功返回 ProductSpec 錯誤返回 nullopt
std::optional<ProductSpec> parse_i010(std::span<const std::byte> buffer) noexcept;

/// @brief 解析 I024 成交揭示
///
/// @param buffer 必須為 I024 訊息格式，內部不會檢查
/// @param spec_table 商品規格表 (需要用於查詢價格小數點位數)
/// @return 成功返回 TradePacket 錯誤返回 nullopt
std::optional<TradePacket> parse_i024(std::span<const std::byte> buffer,
                                      const ProductSpecTable& spec_table) noexcept;

/// @brief 解析 I083 委託簿快照
///
/// @param buffer 必須為 I083 訊息格式，內部不會檢查
/// @param spec_table 商品規格表 (需要用於查詢價格小數點位數)
/// @return 成功返回 BookSnapshot 錯誤返回 nullopt
std::optional<BookSnapshot> parse_i083(std::span<const std::byte> buffer,
                                       const ProductSpecTable& spec_table) noexcept;

/// @brief 解析 I081 委託簿差異更新
///
/// @param buffer 必須為 I081 訊息格式，內部不會檢查
/// @param spec_table 商品規格表 (需要用於查詢價格小數點位數)
/// @return 成功返回 BookUpdate 錯誤返回 nullopt
std::optional<BookUpdate> parse_i081(std::span<const std::byte> buffer,
                                     const ProductSpecTable& spec_table) noexcept;

}  // namespace onyx::net::taifex

#endif
