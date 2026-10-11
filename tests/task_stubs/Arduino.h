#pragma once
#include <stdint.h>
extern uint32_t taskTestClock;
inline uint32_t millis() { return taskTestClock; }
