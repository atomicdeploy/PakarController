#pragma once
#include <stdint.h>
class __FlashStringHelper;
#define F(value) reinterpret_cast<const __FlashStringHelper *>(value)
inline uint32_t millis() { return 0; }
