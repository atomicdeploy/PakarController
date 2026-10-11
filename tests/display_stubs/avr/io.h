#pragma once
#include <Arduino.h>
// Execute the actual __AVR__ PC4/PC5 branch with instrumented port registers.
// Other PORTC bits must stay intact; no hardware voltage/timing claim is made.
struct DisplayRegister {
  bool direction;
  operator uint8_t() const { return direction?display_mock::mode:display_mock::output; }
  DisplayRegister &operator&=(uint8_t value) { display_mock::set(direction,static_cast<uint8_t>(*this)&value); return *this; }
  DisplayRegister &operator|=(uint8_t value) { display_mock::set(direction,static_cast<uint8_t>(*this)|value); return *this; }
};
inline DisplayRegister displayPort{false},displayDirection{true};
#define PORTC displayPort
#define DDRC displayDirection
#define PINC display_mock::input()
#define PORTC4 4
#define PORTC5 5
#define DDC4 4
#define DDC5 5
#define PINC4 4
#define _BV(bit) (1U << (bit))
