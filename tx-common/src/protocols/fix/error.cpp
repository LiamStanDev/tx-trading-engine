#include "tx/protocols/fix/error.hpp"

#include <fmt/format.h>

namespace tx::protocols::fix {

std::string FixCategory::message(int ec) const {
  using enum FixErrc;
  switch (static_cast<FixErrc>(ec)) {
    case InvalidFormat:
      return "Invalid FIX format";
    case InvalidCheckSum:
      return "Invliad checksum";
    case InvalidSeqSum:
      return "Invalid seqsum";
    case MissingBeginString:
      return "Missing BeginString (Tag 8)";
    case MissingBodyLength:
      return "Missing BodyLength (Tag 9)";
    case MissingMsgType:
      return "Missing MsgType (Tag 35)";
    case MissingSender:
      return "Missing Sender (Tag 49)";
    case MissingTarget:
      return "Missing MsgType (Tag 56)";
    case MissingSendingTime:
      return "Missing sending time (Tag 52)";
    case MissingChecksum:
      return "Missing Checksum (Tag 10)";
    case BodyLengthMismatch:
      return "BodyLength mismatch";
    case BodyLengthExceeded:
      return "BodyLength exceeds (should less than 99999)";
    case EmptyMessage:
      return "Empty message";
  }
  return "Unknown FIX parse error";
}

const FixCategory& category() {
  const static FixCategory instance;
  return instance;
}

}  // namespace tx::protocols::fix
