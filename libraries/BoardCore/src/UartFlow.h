#pragma once
#include <stdint.h>

// Build-time transport policy, not a remotely mutable setting. Existing
// ESP8266 profiles remain busy-wire/polling. Buffered mode is paired with the
// S3 hardware UART adapter; it never takes ownership of the busy GPIO.
#ifndef PAKAR_USE_BUSY_WIRE
#define PAKAR_USE_BUSY_WIRE 1
#endif
#ifndef PAKAR_UART_BUFFERED
#define PAKAR_UART_BUFFERED 0
#endif
#if (PAKAR_USE_BUSY_WIRE != 0 && PAKAR_USE_BUSY_WIRE != 1) || \
    (PAKAR_UART_BUFFERED != 0 && PAKAR_UART_BUFFERED != 1)
#error "UART flow flags must be 0 or 1"
#endif
#if PAKAR_USE_BUSY_WIRE && PAKAR_UART_BUFFERED
#error "Buffered UART and busy-wire are mutually exclusive"
#endif
namespace BoardCore {
namespace UartFlow {
constexpr bool unsolicited = PAKAR_USE_BUSY_WIRE || PAKAR_UART_BUFFERED;
// Adapt PCController's entry-occupancy snapshot: also cap large ESP32 rings.
// Leave time for display/audio/keys, even under continuous/noisy RX.
constexpr uint16_t serviceBudget = 64;
inline uint16_t receiveBudget(int available) {
  return available <= 0 ? 0 : available > serviceBudget ? serviceBudget : available;
}
inline bool ready(bool busyLow, int writable, uint8_t frameCapacity) {
  return PAKAR_USE_BUSY_WIRE ? busyLow :
         PAKAR_UART_BUFFERED && writable >= frameCapacity;
}
} // namespace UartFlow
} // namespace BoardCore
