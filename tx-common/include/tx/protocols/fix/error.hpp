#ifndef TX_TRADING_ENGINE_PROTOCOLS_ERROR_HPP
#define TX_TRADING_ENGINE_PROTOCOLS_ERROR_HPP

#include <cstdint>
#include <system_error>
#include <type_traits>

#include "tx/core/error_base.hpp"
#include "tx/core/result.hpp"

namespace tx::protocols::fix {

enum class FixErrc : uint8_t {
  InvalidFormat,       ///< 格式錯誤
  InvalidCheckSum,     ///< Checksum 不正確
  InvalidSeqSum,       ///< SeqSum 格式錯誤
  MissingBeginString,  ///< 缺少 Tag 8
  MissingBodyLength,   ///< 缺少 Tag 9
  MissingMsgType,      ///< 缺少 Tag 35
  MissingSender,       ///< 缺少 Tag 49
  MissingTarget,       ///< 缺少 Tag 56
  MissingSendingTime,  ///< 缺少 Tag 52
  BodyLengthExceeded,  ///< 超過 Body 上限
  MissingChecksum,     ///< 缺少 Tag 10
  BodyLengthMismatch,  ///< BodyLength 與實際不符
  EmptyMessage         ///< 空訊息
};

class FixCategory : public std::error_category {
  const char* name() const noexcept override { return "tx.protocols.fix"; }

  std::string message(int ec) const override;
};

const FixCategory& category();

}  // namespace tx::protocols::fix

template <>
struct std::is_error_code_enum<tx::protocols::fix::FixErrc> : std::true_type {};

template <>
struct tx::core::ErrorTraits<tx::protocols::fix::FixErrc> {
  static std::error_code make_code(tx::protocols::fix::FixErrc ec) {
    return {static_cast<int>(ec), tx::protocols::fix::category()};
  }
};

namespace tx::protocols::fix {

using FixError = tx::core::GenericError<tx::protocols::fix::FixErrc>;

template <typename T = void>
using Result = core::Result<T, FixError>;
}  // namespace tx::protocols::fix

#endif
