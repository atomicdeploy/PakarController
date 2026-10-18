#pragma once
#include "../access_stubs/Arduino.h"
using __FlashStringHelper = char;
inline size_t strlcpy(char *out, const char *value, size_t capacity) {
  const size_t length = strlen(value);
  if (capacity) {
    const size_t count = length < capacity - 1 ? length : capacity - 1;
    memcpy(out, value, count); out[count] = '\0';
  }
  return length;
}
