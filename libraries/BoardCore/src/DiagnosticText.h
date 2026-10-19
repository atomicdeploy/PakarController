#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

namespace BoardCore { namespace DiagnosticText {
// Adapted from ASA0002E.h convertToHexString, getFormattedUptime and
// humanReadableBytes, atomicdeploy/pu850-esp 431104072952f694a35b3a83c19f13fbb098ca5b.
// Owner-directed source reuse; see THIRD_PARTY_NOTICES.md. Keep unsigned byte
// values, fixed caller storage and checked capacity instead of growing Strings.
inline bool encodeHex(const uint8_t *bytes, size_t count, char *out, size_t capacity) {
  if (!out || !capacity) return false;
  out[0] = 0;
  if ((!bytes && count) || count > (capacity - 1) / 2) return false;
  const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < count; ++i) {
    out[i * 2] = hex[bytes[i] >> 4];
    out[i * 2 + 1] = hex[bytes[i] & 15];
  }
  out[count * 2] = 0;
  return true;
}
inline bool formatUptime(uint32_t totalSeconds, char *out, size_t capacity) {
  if (!out || !capacity) return false;
  const unsigned long seconds = totalSeconds % 60;
  const unsigned long minutes = (totalSeconds / 60) % 60;
  const unsigned long hours = (totalSeconds / 3600) % 24;
  const unsigned long days = totalSeconds / 86400;
  const int written = days ? snprintf(out, capacity, "%lu days, %02lu:%02lu:%02lu", days, hours, minutes, seconds) :
      snprintf(out, capacity, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  if (written < 0 || static_cast<size_t>(written) >= capacity) { out[0] = 0; return false; }
  return true;
}
inline bool formatBytes(uint32_t bytes, char *out, size_t capacity) {
  if (!out || !capacity) return false;
  const char *suffixes[] = {"B", "KB", "MB", "GB"};
  double value = bytes;
  size_t unit = 0;
  while (value >= 1024 && unit + 1 < sizeof(suffixes) / sizeof(suffixes[0])) { value /= 1024; ++unit; }
  const int written = snprintf(out, capacity, "%.*f %s", unit > 1 ? 2 : 0, value, suffixes[unit]);
  if (written < 0 || static_cast<size_t>(written) >= capacity) { out[0] = 0; return false; }
  return true;
}
} }
