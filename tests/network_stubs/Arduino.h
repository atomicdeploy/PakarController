#pragma once
// Reuse the existing string fixture; networking adds only a no-op delay.
#include "../access_stubs/Arduino.h"
inline void delay(unsigned long) {}
