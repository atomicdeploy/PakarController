#pragma once

#include <stdint.h>

namespace BoardCore {
namespace WireCodec {

inline uint8_t crc8Update(uint8_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                       : static_cast<uint8_t>(crc << 1);
  }
  return crc;
}

inline uint8_t crc8(const uint8_t *data, uint8_t length) {
  uint8_t crc = 0;
  while (length-- != 0) {
    crc = crc8Update(crc, *data++);
  }
  return crc;
}

// Returns zero on malformed input or insufficient output capacity.
inline uint8_t cobsDecode(const uint8_t *input, uint8_t length,
                          uint8_t *output, uint8_t capacity) {
  uint8_t readIndex = 0;
  uint8_t writeIndex = 0;
  while (readIndex < length) {
    const uint8_t code = input[readIndex++];
    if (code == 0) {
      return 0;
    }
    const uint8_t count = static_cast<uint8_t>(code - 1);
    if (count > static_cast<uint8_t>(length - readIndex) ||
        count > static_cast<uint8_t>(capacity - writeIndex)) {
      return 0;
    }
    for (uint8_t index = 0; index < count; ++index) {
      if (input[readIndex] == 0) return 0;
      output[writeIndex++] = input[readIndex++];
    }
    if (code != 0xFF && readIndex < length) {
      if (writeIndex >= capacity) {
        return 0;
      }
      output[writeIndex++] = 0;
    }
  }
  return writeIndex;
}

// Encodes one delimiter-free packet. The caller appends the zero delimiter.
inline uint8_t cobsEncode(const uint8_t *input, uint8_t length,
                          uint8_t *output, uint8_t capacity) {
  if (capacity == 0) {
    return 0;
  }
  uint8_t readIndex = 0;
  uint8_t writeIndex = 1;
  uint8_t codeIndex = 0;
  uint8_t code = 1;

  while (readIndex < length) {
    if (input[readIndex] == 0) {
      if (codeIndex >= capacity) {
        return 0;
      }
      output[codeIndex] = code;
      code = 1;
      codeIndex = writeIndex++;
      if (writeIndex > capacity) {
        return 0;
      }
      ++readIndex;
      continue;
    }

    if (writeIndex >= capacity) {
      return 0;
    }
    output[writeIndex++] = input[readIndex++];
    ++code;
    if (code == 0xFF) {
      if (codeIndex >= capacity) {
        return 0;
      }
      output[codeIndex] = code;
      code = 1;
      codeIndex = writeIndex++;
      if (writeIndex > capacity) {
        return 0;
      }
    }
  }

  if (codeIndex >= capacity) {
    return 0;
  }
  output[codeIndex] = code;
  return writeIndex;
}

}  // namespace WireCodec
}  // namespace BoardCore
