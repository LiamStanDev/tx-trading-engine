#ifndef TX_TRADING_ENGINE_NETWORK_CONTEXT_ERROR_HPP
#define TX_TRADING_ENGINE_NETWORK_CONTEXT_ERROR_HPP

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include "tx/core/error_base.hpp"
#include "tx/core/result.hpp"

namespace tx::network {

enum class NetworkErrc : uint8_t {
  // Address Error
  INVALID_ADDRESS = 1,
  ADDRESS_FAMILY_NOT_SUPPORTED,
  INVALID_PORT,

  // Socket Error
  SOCKET_CREATE_FAILED,
  INVALID_SOCKET,
  BIND_FAILED,
  LISTEN_FAILED,
  ACCEPT_FAILED,
  CONNECT_FAILED,
  CONNECT_IN_PROGRESS,
  SET_SOCKETOPT_FAILED,
  WOULDBLOCK,
  SEND_FAILED,
  RECV_FAILED,
  INVALID_MULTICAST_ADDR,
  INVALID_INTERFACE_ADDR,
  JOIN_MULTICAST_FAILED,
  LEAVE_MULTICAST_FAILED,
  INVALID_TTL,
  GET_SOCKET_NAME_FAILED,
  GET_PEER_NAME_FAILED,

  // Connection State
  CONNECTION_CLOSED,
  CONNECTION_RESET,
  BROKEN_PIPE,
};

class NetworkCategory : public std::error_category {
  const char* name() const noexcept override { return "tx.network"; }
  std::string message(int ec) const override;
};

const NetworkCategory& category() noexcept;

}  // namespace tx::network

template <>
struct std::is_error_code_enum<tx::network::NetworkErrc> : std::true_type {};

template <>
struct tx::core::ErrorTraits<tx::network::NetworkErrc> {
  static std::error_code make_code(tx::network::NetworkErrc ec) {
    return {static_cast<int>(ec), tx::network::category()};
  }
};

namespace tx::network {

using NetworkError = core::GenericError<NetworkErrc>;

template <typename T = void>
using Result = tx::Result<T, NetworkError>;

}  // namespace tx::network

#endif
