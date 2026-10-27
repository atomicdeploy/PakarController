#pragma once
#include <stdint.h>

namespace BoardCore {
// Shared, allocation-free motion extracted from LedGrid::renderKeyEffects.
// Gain modulates a state's chosen RGB without changing its semantic identity.
// Invalid timing supplied by a caller degrades to solid rather than dividing
// by zero. API validation remains responsible for rejecting invalid settings.
struct StateMotion {
  static uint8_t pulse(uint32_t elapsed, uint32_t period) {
    if (period < 2) return 255;
    const uint32_t phase = elapsed % period, half = period / 2;
    const uint32_t distance = phase < half ? phase : period - phase;
    return uint8_t(255U - 105U * distance / half);
  }
  static uint8_t chase(uint32_t elapsed, uint32_t period,
                       uint16_t position, uint16_t total) {
    if (!period || !total || position >= total) return 255;
    const uint16_t head = uint64_t(elapsed % period) * total / period;
    const uint16_t distance = (head + total - position) % total;
    return distance < 4 ? uint8_t(255 - distance * 45) : 60;
  }
  static uint8_t ring(uint32_t elapsed, uint32_t period, uint16_t sector,
                      uint16_t sectors, bool neighbors, bool invert) {
    if (!sectors || period < sectors || sector >= sectors) return 255;
    const uint32_t phase = elapsed % period;
    const uint32_t position = uint64_t(sector) * period / sectors;
    const uint32_t around = (uint64_t(phase) + period - position) % period;
    const uint32_t distance = around < period - around ? around : period - around;
    const uint32_t reach = (period / sectors) * (neighbors ? 2U : 1U);
    const uint8_t halo = distance < reach ? uint8_t(uint64_t(255) * distance / reach) : 255;
    return invert ? uint8_t(255U - 75U * halo / 255U) : uint8_t(130U + 125U * halo / 255U);
  }
};
}
