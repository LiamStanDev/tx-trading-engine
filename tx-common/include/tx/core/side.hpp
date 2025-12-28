#ifndef TX_TRADING_ENGINE_CORE_SIDE_HPP
#define TX_TRADING_ENGINE_CORE_SIDE_HPP
#include <cstdint>
#include <optional>
#include <string_view>

namespace tx::core {

enum class Side : uint8_t { Buy = 0, Sell = 1 };

inline constexpr Side opposite(Side s) noexcept {
  return (s == Side::Buy) ? Side::Sell : Side::Buy;
}

inline constexpr const char* to_string(Side s) noexcept {
  switch (s) {
    case Side::Buy:
      return "Buy";
    case Side::Sell:
      return "Sell";
  }
  return "Unknown";
}

inline constexpr std::optional<Side> from_string(const std::string_view s) {
  // 這邊為了效率這樣寫
  if (s == "Buy" || s == "buy" || s == "BUY") return Side::Buy;
  if (s == "Sell" || s == "sell" || s == "SELL") return Side::Sell;
  return std::nullopt;
}

}  // namespace tx::core

#endif
