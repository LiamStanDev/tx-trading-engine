#ifndef TX_TRADING_ENGINE_PROTOCOLS_FIX_FIELD_HPP
#define TX_TRADING_ENGINE_PROTOCOLS_FIX_FIELD_HPP

#include <optional>
#include <string_view>
namespace tx::protocols::fix {

/// @brief SOH (Start of Header)
/// @details
///   \x01 表示 16 進制的 1，為 ASCII 的 Non-printable character
inline constexpr char SOH = '\x01';

namespace tags {
inline constexpr int BeginString = 8;
inline constexpr int BodyLength = 9;
inline constexpr int Checksum = 10;
inline constexpr int MsgType = 35;
inline constexpr int SenderCompId = 49;
inline constexpr int TargetCompId = 56;
inline constexpr int MsgSeqSum = 34;
inline constexpr int SendingTime = 52;
inline constexpr int ClOrdID = 11;
inline constexpr int Symbol = 55;
inline constexpr int Side = 54;
inline constexpr int OrderQty = 38;
inline constexpr int OrdType = 40;
inline constexpr int Price = 44;

}  // namespace tags

struct FieldView {
  int tag;
  std::string_view value;

  [[nodiscard]] std::optional<int> to_int() const;
  [[nodiscard]] std::optional<double> to_double() const;
};

}  // namespace tx::protocols::fix

#endif
