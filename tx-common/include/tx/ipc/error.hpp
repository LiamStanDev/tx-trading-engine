#ifndef TX_TRADING_ENGINE_IPC_ERROR_HPP
#define TX_TRADING_ENGINE_IPC_ERROR_HPP

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include "tx/core/error_base.hpp"
#include "tx/core/result.hpp"

namespace tx::ipc {

enum class IpcErrc : uint8_t {
  INVALID_SHM_NAME,
  INVALID_SHM_SIZE,
  SHM_EXISTED,
  SHM_CREATE_FAILED,
  SHM_OPEN_FAILED,
  SHM_NOT_FOUND,
  SHM_PERMISSION_DENY,
};

class IpcCategory : public std::error_category {
  const char* name() const noexcept override { return "tx.ipc"; }
  std::string message(int) const override;
};

const IpcCategory& category() noexcept;

}  // namespace tx::ipc

template <>
struct std::is_error_code_enum<tx::ipc::IpcErrc> : std::true_type {};

template <>
struct tx::core::ErrorTraits<tx::ipc::IpcErrc> {
  static std::error_code make_code(tx::ipc::IpcErrc ec) {
    return {static_cast<int>(ec), tx::ipc::category()};
  }
};

namespace tx::ipc {
using IpcError = core::GenericError<IpcErrc>;

template <typename T = void>
using Result = tx::Result<T, IpcError>;

}  // namespace tx::ipc

#endif
