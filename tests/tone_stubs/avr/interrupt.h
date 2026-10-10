#pragma once
#include <stdint.h>
#define _BV(bit) (1U << (bit))
constexpr uint8_t OCIE1A=1,OCF1A=1,COM1A0=6,WGM12=3,CS10=0,CS11=1,CS12=2;
inline uint8_t SREG=0,TCCR1A=0,TCCR1B=0,TIMSK1=0,TIFR1=0;
inline uint16_t TCNT1=0,OCR1A=0;
inline void cli() {}
