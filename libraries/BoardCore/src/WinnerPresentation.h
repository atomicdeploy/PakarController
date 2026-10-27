#pragma once
#include <stdint.h>

namespace BoardCore { namespace WinnerPresentation {
// One schedule for the AVR cue and ESP pure-color flashes.
constexpr uint16_t firstMs = 240;
constexpr uint16_t secondMs = 240;
constexpr uint16_t finalMs = 360;
constexpr uint16_t gapMs = 120;
constexpr uint16_t durationMs = firstMs + secondMs + finalMs + 2 * gapMs;
inline bool dark(uint32_t elapsed) {
  return (elapsed >= firstMs && elapsed < firstMs + gapMs) ||
         (elapsed >= firstMs + gapMs + secondMs &&
          elapsed < firstMs + secondMs + 2 * gapMs);
}
}}
