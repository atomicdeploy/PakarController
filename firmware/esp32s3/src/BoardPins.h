#pragma once
#include <stdint.h>

// PROVISIONAL common DevKitC-1 plan, not confirmation of the user's module.
// GPIO numbers, never NodeMCU Dx names. Confirm board schematic before wiring.
// UART1 conventional routing: S3 TX17 -> AVR RXD; AVR TXD -> S3 RX18.
// Overrides must match the actual wires; older provisional plans reversed this pair.
#ifndef PAKAR_S3_AVR_RX
#define PAKAR_S3_AVR_RX 18
#endif
#ifndef PAKAR_S3_AVR_TX
#define PAKAR_S3_AVR_TX 17
#endif
#ifndef PAKAR_S3_LED_DATA
#define PAKAR_S3_LED_DATA 4
#endif
#ifndef PAKAR_S3_ISP_RESET
#define PAKAR_S3_ISP_RESET 10
#endif
#ifndef PAKAR_S3_ISP_SCK
#define PAKAR_S3_ISP_SCK 12
#endif
#ifndef PAKAR_S3_ISP_MISO
#define PAKAR_S3_ISP_MISO 13
#endif
#ifndef PAKAR_S3_ISP_MOSI
#define PAKAR_S3_ISP_MOSI 11
#endif

namespace PakarS3 {
constexpr uint8_t kPins[] = {PAKAR_S3_AVR_RX, PAKAR_S3_AVR_TX,
    PAKAR_S3_LED_DATA, PAKAR_S3_ISP_RESET, PAKAR_S3_ISP_SCK,
    PAKAR_S3_ISP_MISO, PAKAR_S3_ISP_MOSI};
constexpr bool safePins() {
  for (unsigned i = 0; i < sizeof(kPins); ++i) {
    // Deliberately conservative: avoid boot straps, USB, module memory,
    // bridge UART, built-in RGB and model-dependent high-numbered pins.
    if (kPins[i] < 4 || kPins[i] > 18) return false;
    for (unsigned j = 0; j < i; ++j) if (kPins[i] == kPins[j]) return false;
  }
  return true;
}
static_assert(safePins(), "S3 pin plan overlaps or needs explicit schematic review");
constexpr uint32_t kAvrBaud = 38400; // Preserve AVR compatibility initially.
constexpr unsigned kAvrRxBuffer = 2048;
constexpr unsigned kAvrTxBuffer = 256;
} // namespace PakarS3
