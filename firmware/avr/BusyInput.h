#pragma once
#include <Arduino.h>
#include "BoardPins.h"

namespace AvrBusyInput {
inline bool ready() {
#if defined(__AVR__) && defined(PINB) && defined(PINB2)
  // This board owns PB2 exclusively for ESP flow control. Like the TM1637
  // direct-port adapter, avoid linking generic Arduino pin/timer dispatch for
  // one fixed input. Never silently apply this optimization to another pin.
  static_assert(BoardPins::EspBusy == PIN_PB2, "Busy input direct-port mapping changed");
  return (PINB & _BV(PINB2)) == 0;
#else
  return digitalRead(BoardPins::EspBusy) == LOW;
#endif
}
} // namespace AvrBusyInput
