#pragma once
#include <stdint.h>
#include <string.h>
class __FlashStringHelper;
#define F(value) reinterpret_cast<const __FlashStringHelper *>(value)
#define pgm_read_byte(address) (*reinterpret_cast<const uint8_t *>(address))
constexpr uint8_t PIN_PC0=14,PIN_PC1=15,PIN_PC2=16,PIN_PC3=17,PIN_PC4=18,PIN_PC5=19;
constexpr uint8_t PIN_PD0=0,PIN_PD1=1,PIN_PD2=2,PIN_PD3=3,PIN_PD4=4,PIN_PD5=5,PIN_PD6=6,PIN_PD7=7;
constexpr uint8_t PIN_PB0=8,PIN_PB1=9,PIN_PB2=10,PIN_PB3=11,PIN_PB4=12,PIN_PB5=13;
constexpr uint8_t OUTPUT=1,INPUT=0,INPUT_PULLUP=2,HIGH=1,LOW=0,A6=20;
inline uint32_t mockNow = 0;
inline bool mockAck = true;
inline unsigned mockReads = 0, mockFailRead = 0;
inline uint8_t mockPinMode[32] = {};
inline void pinMode(uint8_t pin,uint8_t mode) { mockPinMode[pin] = mode; }
inline void digitalWrite(uint8_t,uint8_t) {}
inline int digitalRead(uint8_t pin) {
  if (mockPinMode[pin] == OUTPUT) return HIGH; // Detect ACK contention.
  ++mockReads; return mockAck && mockReads != mockFailRead ? LOW : HIGH;
}
inline void delayMicroseconds(unsigned) {}
inline uint32_t millis() { return mockNow; }
inline uint32_t micros() { return mockNow * 1000UL; }
