#include "tx/network/error.hpp"

#include <fmt/format.h>

namespace tx::network {

std::string NetworkCategory::message(int ec) const {
  using enum NetworkErrc;
  switch (static_cast<NetworkErrc>(ec)) {
    // Address Errors
    case INVALID_ADDRESS:
      return "Invalid IP address";
    case ADDRESS_FAMILY_NOT_SUPPORTED:
      return "Address family not supported";
    case INVALID_PORT:
      return "Invalid port number";

    // Socket Creation & State
    case SOCKET_CREATE_FAILED:
      return "Failed to create socket";
    case INVALID_SOCKET:
      return "Invalid socket (fd < 0)";

    // Connection Errors
    case BIND_FAILED:
      return "Failed to bind socket to address";
    case LISTEN_FAILED:
      return "Failed to listen on socket";
    case ACCEPT_FAILED:
      return "Failed to accept connection";
    case CONNECT_FAILED:
      return "Failed to connect to remote address";
    case CONNECT_IN_PROGRESS:
      return "Connection in progress (non-blocking)";

    // I/O Errors
    case WOULDBLOCK:
      return "Operation would block (non-blocking mode)";
    case SEND_FAILED:
      return "Failed to send data";
    case RECV_FAILED:
      return "Failed to receive data";

    // Socket Options
    case SET_SOCKETOPT_FAILED:
      return "Failed to set socket option";

    // Multicast Errors
    case INVALID_MULTICAST_ADDR:
      return "Invalid multicast address (not in 224.0.0.0/4)";
    case INVALID_INTERFACE_ADDR:
      return "Invalid interface address";
    case JOIN_MULTICAST_FAILED:
      return "Failed to join multicast group";
    case LEAVE_MULTICAST_FAILED:
      return "Failed to leave multicast group";
    case INVALID_TTL:
      return "Invalid TTL value (must be 0-255)";

    // Query Errors
    case GET_SOCKET_NAME_FAILED:
      return "Failed to get local socket address";
    case GET_PEER_NAME_FAILED:
      return "Failed to get remote peer address";

    // Connection State
    case CONNECTION_CLOSED:
      return "Connection closed by peer (received FIN)";
    case CONNECTION_RESET:
      return "Connection reset by peer (received RST)";
    case BROKEN_PIPE:
      return "Broken pipe (write to closed socket)";
  }

  return "Unknown network error";
}

const NetworkCategory& category() noexcept {
  const static NetworkCategory instance;
  return instance;
}

}  // namespace tx::network
