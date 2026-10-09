#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "Protocol.h"

using namespace BoardCore::Protocol;

int main() {
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

  uint8_t damaged[kMaximumEncoded] = {};
  memcpy(damaged, encoded, length);
  damaged[length - 2] ^= 0x40;
  assert(!decode(damaged, length - 1, decoded));

  Frame oversize = {};
  oversize.payloadLength = kMaximumPayload + 1;
  assert(encode(oversize, encoded, sizeof(encoded)) == 0);

  return 0;
}
