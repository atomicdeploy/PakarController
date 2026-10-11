#pragma once
#include <stdint.h>
constexpr uint8_t PIN_PC0=14,PIN_PC1=15,PIN_PC2=16,PIN_PC3=17,PIN_PC4=18,PIN_PC5=19;
constexpr uint8_t PIN_PD0=0,PIN_PD1=1,PIN_PD2=2,PIN_PD3=3,PIN_PD4=4,PIN_PD5=5,PIN_PD6=6,PIN_PD7=7;
constexpr uint8_t PIN_PB0=8,PIN_PB1=9,PIN_PB2=10,PIN_PB3=11,PIN_PB4=12,PIN_PB5=13;
constexpr uint8_t OUTPUT=1,LOW=0;
inline void pinMode(uint8_t,uint8_t) {}
inline void digitalWrite(uint8_t,uint8_t) {}
inline uint32_t millis() { return 0; }
