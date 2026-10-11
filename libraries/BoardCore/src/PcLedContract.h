#pragma once
#include <stdint.h>
#include "Protocol.h"

#ifndef PAKAR_PC_ESP_LED
#define PAKAR_PC_ESP_LED 1
#endif
static_assert(PAKAR_PC_ESP_LED == 0 || PAKAR_PC_ESP_LED == 1, "PAKAR_PC_ESP_LED must be 0 or 1");

namespace BoardCore {
// Adapter of actual PCController ProtocolRuntime::AddressableLed and
// AddressableLeds::configure/stagePixels. No second effects engine or AVR buffer.
// The legacy native contract has an 8-bit index and a 100-pixel maximum;
// full 480-pixel table streaming remains PKAR, never truncated to this window.
template<class Output> class PcLedContract {
 public:
  explicit PcLedContract(Output &output) : output_(output) {}
  Protocol::Error handle(const uint8_t *p, uint8_t n) {
#if PAKAR_PC_ESP_LED
    if (!p || !n) return Protocol::kBadPayload;
    if (n == 2 && p[0] == 0xFE) {
      if (!p[1] || p[1] > 100) return Protocol::kBadPayload;
      output_.clear(); output_.commit(); count_ = p[1];
      return Protocol::kNoError;
    }
    if (n == 1 && p[0] == 0xFC) {
      output_.commit(); return Protocol::kNoError;
    }
    if (n >= 5 && p[0] == 0xFD) {
      if ((n - 2) % 3 || p[1] >= count_ || (n - 2) / 3 > count_ - p[1]) return Protocol::kBadPayload;
      return output_.stage(p[1], p + 2, (n - 2) / 3) ? Protocol::kNoError : Protocol::kHardwareUnavailable;
    }
    if (n != 5 || (p[0] != 0xFF && p[0] >= count_)) return Protocol::kBadPayload;
    // Unlike the donor's inert brightness() stub, use the real shared output.
    output_.brightness(p[4]);
    if (p[0] == 0xFF) {
      for (uint8_t i = 0; i < count_; ++i)
        if (!output_.stage(i, p + 1, 1)) return Protocol::kHardwareUnavailable;
    } else if (!output_.stage(p[0], p + 1, 1)) return Protocol::kHardwareUnavailable;
    output_.commit(); return Protocol::kNoError;
#else
    (void)p; (void)n; return Protocol::kHardwareUnavailable;
#endif
  }
  uint8_t count() const { return count_; }
 private:
  Output &output_;
  uint8_t count_ = 100;
};
}
