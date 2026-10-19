#include <Arduino.h>
#include <cassert>
#include <cstdio>
#if defined(__AVR__)
inline volatile uint8_t mockPortB = 0;
#define PINB mockPortB
#define PINB2 2
#define _BV(bit) (1U << (bit))
#endif
#include "../firmware/avr/BusyInput.h"
int main() {
  mockPinMode[BoardPins::EspBusy] = INPUT_PULLUP;
#if defined(__AVR__)
  for (unsigned value = 0; value <= 255; ++value) {
    mockPortB = static_cast<uint8_t>(value);
    assert(AvrBusyInput::ready() == ((value & (1U << 2)) == 0));
  }
  assert(mockReads == 0); // Fixed PB2 path never invokes generic pin dispatch.
#else
  mockAck = false; assert(!AvrBusyInput::ready()); // HIGH defers unsolicited TX.
  mockAck = true; assert(AvrBusyInput::ready()); // LOW permits the bounded frame.
  assert(mockReads == 2);
#endif
  assert(mockPinMode[BoardPins::EspBusy] == INPUT_PULLUP);
  puts("AVR busy input HIGH/LOW and unrelated-bit isolation passed.");
}
