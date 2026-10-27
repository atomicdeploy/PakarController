#pragma once

#include <stdint.h>
#include <string.h>

#include "WireCodec.h"

namespace BoardCore {
namespace Protocol {

constexpr uint8_t kMagic = 0xA5;
// Fixed reserved byte. Evolution uses opcodes/capabilities, not generations.
constexpr uint8_t kReserved = 0;
constexpr uint8_t kMaximumPayload = 48;
constexpr uint8_t kRawOverhead = 6;
constexpr uint8_t kMaximumRaw = kMaximumPayload + kRawOverhead;
constexpr uint8_t kMaximumEncoded = kMaximumRaw + 2;

enum Opcode : uint8_t {
  kHello = 0x01,
  kGetStatus = 0x02,
  kSetStreamPeriod = 0x03,
  kBuzzer = 0x10,
  kAudioCue = 0x11,
  kAudioStream = 0x12, // Bounded AVR tone ring: action,id16,offset16,packed steps.
  kAudioStreamResponse = 0xBC, // error,id16,nextOffset16,credits,state,muted.
  kDisplayText = 0x38,
  kSetMode = 0x4B,
  kCellAction = 0x4C,
  kGameReset = 0x4D,
  kOutputMask = 0x4E,
  kPing = 0x4F,
  kProgrammingState = 0x50,
  kSetBrightness = 0x51,
  kSetLedPattern = 0x52,
  kNetworkConfig = 0x53,
  kAccessConfig = 0x54, // ESP physical USB only: reset staging, user/password chunks, commit.
  kDeviceName = 0x55, // ESP physical USB: reset(0), label chunks(1), persist/apply(2).
  kPcContract = 0x56, // [PCController opcode, payload]; isolated from legacy 0x11 audio.
  kPcContractChunk = 0x57, // Full 48-byte donor request, bounded two-frame assembly.
  kGetTelemetry = 0x58, // Read-only; does not renew PCController's host/control lease.
  kGetIdentity = 0x59, // ESP USB: [nonce, offset16], bounded read-only identity chunks.
  kEspPanel = 0x5A, // Paired ESP settings: state offer or AVR physical-menu action.
  kAvrRxDiagnostic = 0x5B, // Physical ESP USB only, quiet S3 build: bounded RX-only capture.
  kSetLedOutput = 0x5C, // ESP only: one boolean; independent of presentation/game mode.
  kGetTableSnapshot = 0x5D, // USB [nonce,offset16,token32]; zero token/offset captures immutable table state.
  kTableCommand = 0x5E, // USB [boot_id16,inner opcode,<=31 bytes]; bounded table-only, ACK names inner opcode.
  kCellsAction = 0x5F, // ESP only: [state,effect,10-byte logical-cell bitmap], one atomic loop-owner update.
  kShellRequest = 0x60, // ESP only: boot-fenced Open, token/sequence Input, Close and bounded output Poll.
  kPeerReport = 0x61, // USB: [totalLE16,offsetLE16,<=44 JSON bytes], bounded sequential peer introduction.
  kCellAssignments = 0x62, // ESP table-selection: selection bitmap10 + two-bit states20; one atomic transaction.
  kCellLock = 0x63, // ESP table-selection: enabled boolean; persistent red-to-white guard.
  kHostDemand = 0x64, // ESP USB: [flagsLE16,leaseMsLE16]; zero pair releases optional observations.
  kPixelDiagnostic = 0x65, // Boot-fenced table inner: [indexLE16,count,R,G,B,off]; one pixel or six aligned pixels.
  kKeyMask = 0x66, // Paired first-wins AVR: keys LE16, optional winner/reset cue bytes when HELLO[16] ACKs support; 255=silent.
  kKeySettings = 0x67, // ESP fenced patch: mask8, keys16, RGB24 x3, effects8, autoReset32; optional winner/reset bytes17/18 via mask64/128.
  kGamePress = 0x68, // One enabled key (1..9), same first-winner latch as physical Down.
  kKeyPresentation = 0x69, // Fenced ESP patch: global [0,count,on16,gap16,fade16,pulse16,chase16] or group [1,id,override,winRGB24,loseRGB24,winMotion,loseMotion].
  kUsbApi = 0x6A, // Bounded boot-fenced API transaction; see UsbApiRequest.h.
  kUsbApiResponse = 0xBD, // [action,token32,total16,offset16,<=39 response bytes].
  kHostDemandResponse = 0xBA, // Correlated [effectiveFlagsLE16,remainingMsLE16], not an actuator ACK.
  kShellResponse = 0xB9, // Encapsulated terminal output; no raw bytes interleaved with COBS.
  kTableSnapshotResponse = 0xB6, // [nonce,token32,total16,offset16,<=39 immutable JSON bytes].
  kTableCommandState = 0xB7, // Post-ACK readback: inner opcode, boot16, 7 scalar fields, 80 packed two-bit cell states.
  kPanelEvent = 0xB8, // ESP semantic presentation event; requested, NOT confirmed segment rendering.
  kAvrRxDiagnosticResponse = 0xB4, // Action/error + actual capture/pad observations; never AVR ACK.
  kResetTelemetryResponse = 0xB5, // [MCUSR cause,countLE32,journal_verified]; optional GET_TELEMETRY companion.
  kEspPanelResponse = 0xB3, // Correlated action, error and authoritative ESP readback.
  kIdentityResponse = 0xB2, // [nonce, revision32, total16, offset16, <=39 JSON bytes].
  kTelemetryResponse = 0xB1, // 46 bytes; actual AVR observations.
  kPcContractResponse = 0xB0, // [PCController response opcode, payload].

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
  kRedCellsLocked = 7,
  kInvalidCellAssignments = 8,
  kUnsupportedBoardRole = 9,
};

enum DeviceRole : uint8_t { kRoleAvr = 1, kRoleEsp = 2, kRoleHost = 3 };

namespace UsbDemand {
// Explicit command/read replies never depend on these optional stream flags.
// Pixels only enriches a requested immutable table capture, never starts a push.
enum Flag : uint16_t { Status = 1, Panel = 2, Peripheral = 4, Pixels = 8 };
constexpr uint16_t kAllowedFlags = Status | Panel | Peripheral | Pixels;
constexpr uint16_t kMinimumLeaseMs = 250;
constexpr uint16_t kMaximumLeaseMs = 5000;
} // namespace UsbDemand

enum GameMode : uint8_t {
  kModeIdle = 0,
  kModeCellSelect = 1,
  kModeFirstWins = 2,
  kModeSnake = 3,
};
// Existing stored codes remain stable; appearance is independent of identity.
enum CellState : uint8_t { kCellWhite = 0, kCellRed = 1, kCellOff = 2,
  kCellWon = 3, kCellIdle = kCellWhite, kCellLost = kCellRed };
enum Effect : uint8_t { kEffectImmediate = 0, kEffectFade = 1, kEffectPulse = 2 };

enum AudioCue : uint8_t {
  kCueWelcome = 0,
  kCueUpdateStart = 1,
  kCueUpdateSuccess = 2,
  kCueError = 3,
  kCueWinner = 4,
  kCueFinish = 5,
  kCueLost = 6,
  kCueIncorrect = 7,
  kCueRapidError = 8,
  kCueFault = 9,
  kCueIncrease = 10,
  kCueDecrease = 11,
  kCueWelcomePuzzleBoard = 12,
  kCueWelcome40Led = 13,
  kCueAttention = 14,
  kCueCount = 15,
};

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
  kCapWebSocket = 1U << 10,
  kCapNtp = 1U << 11,
  kCapBusyWire = 1U << 12,
  kCapHostSerial = 1U << 13,
  kCapDisplayText = 1U << 14, // Individual HELLO acknowledgement of the text adapter.
  kCapEspPanel = 1U << 15, // Explicit paired ESP settings/front-panel adapter.
};

namespace EspPanel {
enum Action : uint8_t { Offer = 0, Query = 16, SetMode = 17,
  Reconnect = 18, Scan = 19, SetDhcp = 20 };
enum Notice : uint8_t { Current = 0, Draft = 1, Pending = 2,
  Saved = 3, Failed = 4, TimedOut = 5 };
struct Snapshot {
  uint8_t mode = 1; // 1 AP, 2 station, 3 both; zero is never a safe network mode.
  bool dhcp = true, stationConfigured = false, stationConnected = false;
  char hostname[33] = {};
};
inline uint8_t encodeSnapshot(const Snapshot &state, uint8_t *out) {
  uint8_t size = 0;
  while (size < 32 && state.hostname[size]) ++size;
  out[0] = state.mode; out[1] = state.dhcp;
  out[2] = state.stationConfigured; out[3] = state.stationConnected;
  out[4] = size;
  memcpy(out + 5, state.hostname, size);
  return 5 + size;
}
inline bool decodeSnapshot(const uint8_t *p, uint8_t size, Snapshot &state) {
  if (!p || size < 6 || p[0] < 1 || p[0] > 3 || p[1] > 1 ||
      p[2] > 1 || p[3] > 1 || !p[4] || p[4] > 32 || size != 5 + p[4])
    return false;
  for (uint8_t i = 0; i < p[4]; ++i) {
    const uint8_t ch = p[5 + i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || (ch == '-' && i && i + 1 < p[4])))
      return false;
  }
  state.mode = p[0]; state.dhcp = p[1];
  state.stationConfigured = p[2]; state.stationConnected = p[3];
  memcpy(state.hostname, p + 5, p[4]); state.hostname[p[4]] = 0;
  return true;
}
} // namespace EspPanel

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
  raw[1] = kReserved;
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
      raw[1] != kReserved ||
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
