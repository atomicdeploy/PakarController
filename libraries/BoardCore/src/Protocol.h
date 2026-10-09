#pragma once

#include <stdint.h>
#include <string.h>

#include "WireCodec.h"

namespace BoardCore {
namespace Protocol {

constexpr uint8_t kMagic = 0xA5;
constexpr uint8_t kEnvelopeSchema = 1;
constexpr uint8_t kMaximumPayload = 48;
constexpr uint8_t kRawOverhead = 6;
constexpr uint8_t kMaximumRaw = kMaximumPayload + kRawOverhead;
constexpr uint8_t kMaximumEncoded = kMaximumRaw + 2;

enum Opcode : uint8_t {
  kHello = 0x01,
  kGetStatus = 0x02,
  kSetStreamPeriod = 0x03,
  kBuzzer = 0x10,
  kDisplayText = 0x38,
  kSetMode = 0x4B,
  kCellAction = 0x4C,
  kGameReset = 0x4D,
  kOutputMask = 0x4E,
  kPing = 0x4F,

  kAck = 0x80,
  kHelloResponse = 0x81,
  kErrorResponse = 0x82,
  kStatusResponse = 0x90,
  kGameState = 0xA3,
  kInputEvent = 0xA4,
  kCellEvent = 0xA5,
  kPong = 0xA6,
};

enum Error : uint8_t {
  kNoError = 0,
  kBadEnvelope = 1,
  kUnsupported = 2,
  kBadPayload = 3,
  kHardwareUnavailable = 4,
  kBusy = 5,
  kUnsafe = 6,
};

enum DeviceRole : uint8_t { kRoleAvr = 1, kRoleEsp = 2, kRoleHost = 3 };
enum GameMode : uint8_t { kModeIdle = 0, kModeCellSelect = 1, kModeFirstWins = 2 };
enum CellState : uint8_t { kCellWhite = 0, kCellRed = 1, kCellOff = 2 };
enum Effect : uint8_t { kEffectImmediate = 0, kEffectFade = 1, kEffectPulse = 2 };

enum Capability : uint16_t {
  kCapShiftInputs = 1U << 0,
  kCapShiftOutputs = 1U << 1,
  kCapAdc9 = 1U << 2,
  kCapTm1637 = 1U << 3,
  kCapBuzzer = 1U << 4,
  kCapLedGrid = 1U << 5,
  kCapIsp = 1U << 6,
  kCapOta = 1U << 7,
  kCapLittleFs = 1U << 8,
  kCapApi = 1U << 9,
};

struct Frame {
  uint8_t opcode;
  uint8_t sequence;
  uint8_t payloadLength;
  uint8_t payload[kMaximumPayload];
};

inline uint8_t encode(const Frame &frame, uint8_t *output, uint8_t capacity) {
  if (frame.payloadLength > kMaximumPayload || capacity < 2) return 0;
  uint8_t raw[kMaximumRaw];
  raw[0] = kMagic;
  raw[1] = kEnvelopeSchema;
  raw[2] = frame.opcode;
  raw[3] = frame.sequence;
  raw[4] = frame.payloadLength;
  if (frame.payloadLength != 0) {
    memcpy(raw + 5, frame.payload, frame.payloadLength);
  }
  const uint8_t rawLength = frame.payloadLength + kRawOverhead;
  raw[rawLength - 1] = WireCodec::crc8(raw, rawLength - 1);
  const uint8_t encoded = WireCodec::cobsEncode(
      raw, rawLength, output, static_cast<uint8_t>(capacity - 1));
  if (encoded == 0 || encoded >= capacity) return 0;
  output[encoded] = 0;
  return encoded + 1;
}

inline bool decode(const uint8_t *input, uint8_t length, Frame &frame) {
  uint8_t raw[kMaximumRaw];
  const uint8_t rawLength = WireCodec::cobsDecode(input, length, raw, sizeof(raw));
  if (rawLength < kRawOverhead || raw[0] != kMagic ||
      raw[1] != kEnvelopeSchema ||
      raw[4] > kMaximumPayload || rawLength != raw[4] + kRawOverhead ||
      raw[rawLength - 1] != WireCodec::crc8(raw, rawLength - 1)) {
    return false;
  }
  frame.opcode = raw[2];
  frame.sequence = raw[3];
  frame.payloadLength = raw[4];
  if (frame.payloadLength != 0) memcpy(frame.payload, raw + 5, frame.payloadLength);
  return true;
}

}  // namespace Protocol
}  // namespace BoardCore
