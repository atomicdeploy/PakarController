#pragma once
#include <stddef.h>
#include <string.h>
namespace BoardCore { namespace Identity {
// One DNS label, optionally qualified with .local. No URLs or arbitrary domains.
inline bool normalizeName(const char *input, char (&output)[64]) {
  if (!input) return false;
  size_t length = strlen(input);
  if (length > 69) return false;
  char lower[70] = {};
  for (size_t i = 0; i < length; ++i) {
    const char c = input[i];
    lower[i] = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
  }
  if (length >= 6 && strcmp(lower + length - 6, ".local") == 0) length -= 6;
  if (!length || length > 63 || lower[0] == '-' || lower[length - 1] == '-') return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = lower[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  lower[length] = 0;
  if (strcmp(lower, "localhost") == 0 || strcmp(lower, "local") == 0) return false;
  memset(output, 0, sizeof(output)); memcpy(output, lower, length); return true;
}
} }
