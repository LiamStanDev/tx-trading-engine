#include <iostream>

#include "tx/core/result.hpp"

#if __cplusplus >= 202302L && __has_include(<expected>)
#include <expected>
#define HAS_STD_EXPECTED 1
#else
#define HAS_STD_EXPECTED 0
#endif

struct TradeData {
  uint64_t order_id;
  double price;
  uint32_t quantity;
};

enum class ErrorCode : uint32_t {
  None = 0,
  InvalidPrice,
  InvalidQuantity,
  RiskLimitExceeded
};

struct CResult {
  TradeData data;
  ErrorCode err;
};

int main() {
  std::cout << "=== Memory Layout Comparison ===" << std::endl;
  std::cout << "sizeof(TradeData):              " << sizeof(TradeData)
            << " bytes" << std::endl;
  std::cout << "sizeof(ErrorCode):              " << sizeof(ErrorCode)
            << " bytes" << std::endl;
  std::cout << std::endl;

  std::cout << "sizeof(CResult):                " << sizeof(CResult) << " bytes"
            << std::endl;
  std::cout << "sizeof(tx::Result<...>):        "
            << sizeof(tx::Result<TradeData, ErrorCode>) << " bytes"
            << std::endl;

#if HAS_STD_EXPECTED
  std::cout << "sizeof(std::expected<...>):     "
            << sizeof(std::expected<TradeData, ErrorCode>) << " bytes"
            << std::endl;
#else
  std::cout << "sizeof(std::expected<...>):     N/A (C++23 not available)"
            << std::endl;
#endif

  std::cout << std::endl;
  std::cout << "Memory overhead:" << std::endl;
  std::cout << "CResult overhead:               "
            << (sizeof(CResult) - sizeof(TradeData)) << " bytes" << std::endl;
  std::cout << "tx::Result overhead:            "
            << (sizeof(tx::Result<TradeData, ErrorCode>) - sizeof(TradeData))
            << " bytes" << std::endl;
#if HAS_STD_EXPECTED
  std::cout << "std::expected overhead:         "
            << (sizeof(std::expected<TradeData, ErrorCode>) - sizeof(TradeData))
            << " bytes" << std::endl;
#endif

  return 0;
}
