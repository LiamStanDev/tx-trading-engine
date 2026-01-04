#include "tx/protocols/fix/error.hpp"

#include <fmt/format.h>

namespace tx::protocols::fix {

std::string FixParseCategory::message(int ec) const {
  using enum FixParseErrc;
  switch (static_cast<FixParseErrc>(ec)) {
    case InvalidFormat:
      return "Invalid FIX format";
    case InvalidCheckSum:
      return "Invliad checksum";
    case MissingBeginString:
      return "Missing BeginString (Tag 8)";
    case MissingBodyLength:
      return "Missing BodyLength (Tag 9)";
    case MissingMsgType:
      return "Missing MsgType (Tag 35)";
    case MissingChecksum:
      return "Missing Checksum (Tag 10)";
    case BodyLengthMismatch:
      return "BodyLength mismatch";
    case EmptyMessage:
      return "Empty message";
  }
  return "Unknown FIX parse error";
}

const FixParseCategory& category() {
  const static FixParseCategory instance;
  return instance;
}

}  // namespace tx::protocols::fix
