#pragma once
#include <stdint.h>
#include <stddef.h>

namespace PakarS3 {
#ifndef PAKAR_S3_LED_SWAP_RED_BLUE
#define PAKAR_S3_LED_SWAP_RED_BLUE 1
#endif
static_assert(PAKAR_S3_LED_SWAP_RED_BLUE==0 || PAKAR_S3_LED_SWAP_RED_BLUE==1,"LED channel swap must be boolean");
// API/effect buffers remain logical RGB; this board's wire order swaps R/B.
// WS2811 V1.4 p3: T0H220..380ns, T1H/T0L/T1L580..1000ns.
// RMT:300/950ns zero,625/625ns one,1250ns period. Avoid the previous
// zero LOW exactly at1000ns and one HIGH only20ns above its minimum.
// Static GPIO retains its existing240MHz cycle profile, NOT a hidden change
// to its unmeasured store/loop overhead. No per-frame temporal dithering.
inline uint8_t wireChannel(uint8_t value,uint8_t brightness,uint8_t correction) {
  return static_cast<uint8_t>((uint32_t(value)*brightness*correction+32512U)/65025U);
}
inline constexpr uint16_t zeroHigh=12,zeroLow=38,oneHigh=25,oneLow=25;
static_assert(zeroHigh+zeroLow==50 && oneHigh+oneLow==50,"WS2811 bit must be1250ns at40MHz");
inline constexpr uint16_t resetHalf=6000; //300us low total, including latch.
inline constexpr uint16_t resetMicros=300;
inline constexpr uint16_t staticBitCycles=300,staticZeroHighCycles=60,staticOneHighCycles=144;
template<class Rgb>
void encodeWireBytes(const Rgb *rgb,size_t count,uint8_t brightness,bool enabled,uint8_t *out){
  const uint8_t correction[]={255,176,240};
  for(size_t p=0;p<count;++p){
    const uint8_t raw[]={rgb[p].r,rgb[p].g,rgb[p].b};
    for(size_t c=0;c<3;++c){
      const size_t logical=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
      out[p*3+c]=enabled?wireChannel(raw[logical],brightness,correction[logical]):0;
    }
  }
}
template<class Rgb,class Symbol>
void encodeWire(const Rgb *rgb,size_t count,uint8_t brightness,bool enabled,Symbol *out){
  size_t n=0;const uint8_t correction[]={255,176,240};
  for(size_t p=0;p<count;++p){
    const uint8_t raw[]={rgb[p].r,rgb[p].g,rgb[p].b};
    for(size_t c=0;c<3;++c){
      const size_t logical=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
      const uint8_t value=enabled?wireChannel(raw[logical],brightness,correction[logical]):0;
      for(uint8_t bit=0x80;bit;bit>>=1){auto &s=out[n++];const bool one=value&bit;
        s.level0=1;s.duration0=one?oneHigh:zeroHigh;s.level1=0;s.duration1=one?oneLow:zeroLow;}
    }
  }
  out[n].level0=0;out[n].duration0=resetHalf;out[n].level1=0;out[n].duration1=resetHalf;
}
}
