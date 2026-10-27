// SPDX-License-Identifier: LGPL-3.0-or-later
// D6 sender adapted from PCController Project/AddressableLeds.cpp, itself
// derived from Adafruit NeoPixel's 16 MHz AVR 20-cycle sender.
#pragma once
#ifndef PAKAR_AVR_LED_DIAGNOSTIC
#define PAKAR_AVR_LED_DIAGNOSTIC 0
#endif
#if PAKAR_AVR_LED_DIAGNOSTIC
#include <avr/interrupt.h>
#include <util/delay.h>
namespace LedDiagnostic {
static_assert(F_CPU == 16000000UL, "Diagnostic sender requires 16 MHz AVR");
inline void begin(){pinMode(BoardPins::AddressableLed,OUTPUT);digitalWrite(BoardPins::AddressableLed,LOW);}
inline void service(uint32_t now,uint8_t winner,bool refreshAllowed){
  static uint32_t last=0;
  static uint8_t previousWinner=255;
  // Extra byte makes the assembly's final speculative load valid.
  static uint8_t pixels[48*3+1]={};
  const bool changed=winner!=previousWinner;
  if(!changed && (!refreshAllowed || uint32_t(now-last)<250))return;
  last=now;
  previousWinner=winner;
  for(uint8_t i=0;i<48;++i){
    const uint8_t box=i<24?1:2;
    // GRB bytes, identical semantic white/green/red to the S3 two-box frame.
    pixels[i*3]=(!winner || winner==box)?255:0;
    pixels[i*3+1]=(!winner || winner!=box)?255:0;
    pixels[i*3+2]=winner?0:255;
  }
  volatile uint8_t *port=&PORTD;
  const uint8_t hi=*port|_BV(PORTD6),lo=*port&~_BV(PORTD6);
  const uint8_t *ptr=pixels;
  uint16_t count=48*3;
  uint8_t byte=*ptr++,next=lo,bit=8;
  const uint8_t saved=SREG;cli();
  asm volatile(
    "head20%=:" "\n\t"
    "st %a[port], %[hi]" "\n\t"
    "sbrc %[byte], 7" "\n\t"
    "mov %[next], %[hi]" "\n\t"
    "dec %[bit]" "\n\t"
    "st %a[port], %[next]" "\n\t"
    "mov %[next], %[lo]" "\n\t"
    "breq nextbyte20%=" "\n\t"
    "rol %[byte]" "\n\t"
    "rjmp .+0" "\n\t" "nop" "\n\t"
    "st %a[port], %[lo]" "\n\t"
    "nop" "\n\t" "rjmp .+0" "\n\t"
    "rjmp head20%=" "\n\t"
    "nextbyte20%=:" "\n\t"
    "ldi %[bit], 8" "\n\t"
    "ld %[byte], %a[ptr]+" "\n\t"
    "st %a[port], %[lo]" "\n\t"
    "nop" "\n\t" "sbiw %[count], 1" "\n\t"
    "brne head20%=" "\n"
    : [port] "+e"(port),[byte] "+r"(byte),[bit] "+r"(bit),
      [next] "+r"(next),[count] "+w"(count),[ptr] "+e"(ptr)
    : [hi] "r"(hi),[lo] "r"(lo) : "memory");
  SREG=saved;
  _delay_us(300);
}
}
#endif
