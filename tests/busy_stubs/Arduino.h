#pragma once
#include <stdint.h>
constexpr uint8_t INPUT_PULLUP = 2, OUTPUT = 3, LOW = 0;
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
