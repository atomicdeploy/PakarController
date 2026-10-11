#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <initializer_list>

#include "Protocol.h"
#include "ApiPolicy.h"
#include "PeerFence.h"

using namespace BoardCore::Protocol;

int main() {
  // Adapt donor matching-identity/request assertions to an actual boot fence.
  // Donor session.go HELLO build metadata itself is NOT a boot epoch.
  using BoardCore::PeerFence::accepts;
  const char *board = "esp8266-aabbccddeeff";
  const char *boot = "123456789abcdef0123456789abcdef0";
  assert(accepts(board, boot, nullptr, nullptr)); // Explicit legacy/unfenced.
  assert(accepts(board, boot, board, boot));
  assert(!accepts(board, boot, board, nullptr));
  assert(!accepts(board, boot, nullptr, boot));
  assert(!accepts(board, boot, "", boot));
  assert(!accepts(board, boot, board, "223456789abcdef0123456789abcdef0"));
  assert(!accepts(board, boot, "esp8266-aabbccddee00", boot));
  assert(!accepts(board, "", board, boot));
  assert(!accepts(board, boot, board, "00000000000000000000000000000000"));
  assert(!accepts(board, boot, board, "123456789ABCDEF0123456789abcdef0"));
  assert(!BoardCore::PeerFence::boardValid("bad\r\nheader"));
  assert(!BoardCore::PeerFence::boardValid(""));
  // Actual donor TestVersionedCoreAPIRoutesAreRejected cases, plus vN bounds.
  for (const char *path : {"/api/v1", "/api/v1/rpc", "/api/v1/snapshot",
       "/api/v1/peripherals", "/api/v2/status", "/api/v123/", "/api/v1?x=1"}) {
    assert(BoardCore::ApiPolicy::numberedRoute(path));
  }
  for (const char *path : {"/api/status", "/api/device/name", "/api/firmware/fetch",
       "/api/version", "/api/v1other", "/ws", "/"}) {
    assert(!BoardCore::ApiPolicy::numberedRoute(path));
  }
  static_assert(kAudioCue == 0x11, "audio cue opcode changed");
  static_assert(kProgrammingState == 0x50,
                "programming latch opcode changed");
  static_assert(kSetBrightness == 0x51 && kSetLedPattern == 0x52,
                "host LED control opcodes changed");
  static_assert(kNetworkConfig == 0x53,
                "USB network recovery opcode changed");
  static_assert((kCapWebSocket | kCapNtp | kCapBusyWire | kCapHostSerial) ==
                    0x3C00,
                "ESP transport capability layout changed");
  static_assert(kCueUpdateStart != kCueUpdateSuccess,
                "update cues must be distinguishable");

  Frame original = {};
  original.opcode = kCellAction;
  original.sequence = 0;
  original.payloadLength = kMaximumPayload;
  for (uint8_t index = 0; index < original.payloadLength; ++index) {
    original.payload[index] = index % 7 == 0 ? 0 : index;
  }

  uint8_t encoded[kMaximumEncoded] = {};
  const uint8_t length = encode(original, encoded, sizeof(encoded));
  assert(length > 1);
  assert(encoded[length - 1] == 0);

  Frame decoded = {};
  assert(decode(encoded, length - 1, decoded));
  assert(decoded.opcode == original.opcode);
  assert(decoded.sequence == original.sequence);
  assert(decoded.payloadLength == original.payloadLength);
  assert(memcmp(decoded.payload, original.payload, original.payloadLength) == 0);
  uint8_t raw[kMaximumRaw] = {};
  const uint8_t rawLength = BoardCore::WireCodec::cobsDecode(encoded, length - 1, raw, sizeof(raw));
  assert(raw[1] == 0); // Fixed reserved byte, not a contract generation.
  raw[1] = 1;
  raw[rawLength - 1] = BoardCore::WireCodec::crc8(raw, rawLength - 1);
  const uint8_t numbered = BoardCore::WireCodec::cobsEncode(raw, rawLength, encoded, sizeof(encoded));
  assert(!decode(encoded, numbered, decoded)); // Valid CRC cannot enable another generation.

  uint8_t damaged[kMaximumEncoded] = {};
  memcpy(damaged, encoded, length);
  damaged[length - 2] ^= 0x40;
  assert(!decode(damaged, length - 1, decoded));

  Frame oversize = {};
  oversize.payloadLength = kMaximumPayload + 1;
  assert(encode(oversize, encoded, sizeof(encoded)) == 0);

  return 0;
}
