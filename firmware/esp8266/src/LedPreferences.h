#pragma once

#include "LedGrid.h"

// Retain the existing private six-byte file format. Effects are runtime choices,
// not boot choices: even a valid older rainbow/fire record boots a white table.
struct LedPreferences {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t schema;
  uint8_t pattern;
  uint8_t brightness;
  uint8_t check;
};
static_assert(sizeof(LedPreferences) == 6, "LED preferences file layout changed");

inline uint8_t ledPreferencesCheck(const LedPreferences &value) {
  return static_cast<uint8_t>(value.magic0 ^ value.magic1 ^ value.schema ^
                              value.pattern ^ value.brightness ^ 0xA5U);
}

inline LedPreferences whiteBootPreferences(uint8_t brightness) {
  LedPreferences value{0x50, 0x4C, 1, LedGrid::kPatternNone, brightness, 0};
  value.check = ledPreferencesCheck(value);
  return value;
}

inline bool applyLedBootPreferences(LedGrid &grid, const LedPreferences &value) {
  if (value.magic0 != 0x50 || value.magic1 != 0x4C || value.schema != 1 ||
      value.pattern > LedGrid::kPatternFire || value.brightness == 0 ||
      value.check != ledPreferencesCheck(value)) return false;
  grid.setBrightness(value.brightness);
  grid.resetWhite();
  return true;
}
