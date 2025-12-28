#ifndef TX_TRADING_ENGINE_NETWORK_ADDRESS_ERROR_HPP
#define TX_TRADING_ENGINE_NETWORK_ADDRESS_ERROR_HPP

#include <cstdint>
#include <string>

namespace tx::network {

enum class AddressErrorCode : uint8_t {
  InvalidFormat,      // IP 格式錯誤
  InvalidPort,        // 埠號超出範圍
  ParseFailure,       // 解析失敗
  UnsupportedFamily,  // 不支援的協定家族
};

class AddressError {
 private:
  AddressErrorCode code_;
  std::string detail_;

 public:
  AddressError(AddressErrorCode code, std::string detail = "") noexcept
      : code_(code), detail_(std::move(detail)) {}

  [[nodiscard]] AddressErrorCode code() const noexcept { return code_; }
  [[nodiscard]] const std::string& detail() const noexcept { return detail_; }
  [[nodiscard]] const char* code_name() const noexcept;
  [[nodiscard]] std::string message() const noexcept;
};

}  // namespace tx::network

#endif
