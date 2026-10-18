#pragma once

#include "Protocol.h"

namespace BoardCore {
// COBS/CRC proves packet integrity, not that a peer has identified itself or
// supplied a meaningful heartbeat. Keep this independent of transport clocks.
inline bool validAvrHello(const Protocol::Frame &frame) {
  return frame.opcode == Protocol::kHelloResponse && frame.payloadLength >= 9 &&
      frame.payload[0] == 0 && frame.payload[1] == Protocol::kRoleAvr;
}
inline bool validAvrObservation(const Protocol::Frame &frame, bool identified) {
  using namespace Protocol;
  if (frame.opcode == kHelloResponse) return validAvrHello(frame);
  if (!identified) return false;
  switch (frame.opcode) {
    case kTelemetryResponse: return frame.payloadLength == 46 && frame.payload[0] == 0;
    case kResetTelemetryResponse: return frame.payloadLength == 6 && frame.payload[5] <= 1;
    case kGameState:
      return frame.payloadLength >= 4 && frame.payload[0] <= kModeSnake &&
          frame.payload[1] <= 9 && frame.payload[2] >= 1 && frame.payload[2] <= 80 &&
          frame.payload[3] <= kCellOff;
    case kPcContractResponse: return frame.payloadLength >= 1;
    case kAck:
    case kErrorResponse: return frame.payloadLength >= 2 && frame.payload[1] <= kUnsafe;
    case kPong: return true; // Payload is the caller's opaque ping value.
    case kEspPanel: return frame.payloadLength >= 1;
    default: return false;
  }
}
} // namespace BoardCore
