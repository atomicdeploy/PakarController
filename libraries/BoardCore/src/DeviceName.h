#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace BoardCore { namespace Identity {
// UTF-8 board labels are separate from DNS labels. Reject controls, malformed
// UTF-8, overlong encodings, surrogates and code points beyond Unicode.
inline bool validDisplayName(const char *input) {
  if (!input) return false;
  const size_t length = strlen(input);
  if (!length || length > 63 || input[0] == ' ' || input[length - 1] == ' ') return false;
  for (size_t i = 0; i < length;) {
    const uint8_t lead = static_cast<uint8_t>(input[i++]);
    if (lead < 0x80) { if (lead < 0x20 || lead == 0x7f) return false; continue; }
    const uint8_t count = lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
    if (!count || i + count > length) return false;
    const uint8_t second = static_cast<uint8_t>(input[i]);
    if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second >= 0xa0) ||
        (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second >= 0x90) ||
        (lead == 0xc2 && second < 0xa0)) return false; // C1 controls too.
    for (uint8_t n = 0; n < count; ++n) if ((static_cast<uint8_t>(input[i++]) & 0xc0) != 0x80) return false;
  }
  return true;
}
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
