#ifndef TX_TRADING_ENGINE_PROTOCOLS_ERROR_HPP
#define TX_TRADING_ENGINE_PROTOCOLS_ERROR_HPP

#include <cstdint>
#include <system_error>
#include <type_traits>

#include "tx/core/error_base.hpp"
#include "tx/core/result.hpp"

namespace tx::protocols::fix {

enum class FixParseErrc : uint8_t {
  InvalidFormat,       ///< 格式錯誤
  InvalidCheckSum,     ///< Checksum 不正確
  MissingBeginString,  ///< 缺少 Tag 8
  MissingBodyLength,   ///< 缺少 Tag 9
  MissingMsgType,      ///< 缺少 Tag 35
  MissingChecksum,     ///< 缺少 Tag 10
  BodyLengthMismatch,  ///< BodyLength 與實際不符
  EmptyMessage         ///< 空訊息
};

class FixParseCategory : public std::error_category {
  const char* name() const noexcept override { return "tx.protocols.fix"; }

  std::string message(int ec) const override;
};

const FixParseCategory& category();

}  // namespace tx::protocols::fix

template <>
struct std::is_error_code_enum<tx::protocols::fix::FixParseErrc>
    : std::true_type {};

template <>
struct tx::core::ErrorTraits<tx::protocols::fix::FixParseErrc> {
  static std::error_code make_code(tx::protocols::fix::FixParseErrc ec) {
    return {static_cast<int>(ec), tx::protocols::fix::category()};
  }
};

namespace tx::protocols::fix {

using FixParseError = tx::core::GenericError<tx::protocols::fix::FixParseErrc>;

template <typename T = void>
using Result = core::Result<T, FixParseError>;
}  // namespace tx::protocols::fix

#endif
