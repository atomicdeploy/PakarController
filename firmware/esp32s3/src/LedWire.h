#pragma once
#include <stdint.h>
#include <stddef.h>
#include <GameRole.h>

namespace PakarS3 {
#ifndef PAKAR_S3_LED_SWAP_RED_BLUE
#define PAKAR_S3_LED_SWAP_RED_BLUE PAKAR_TABLE_SELECTION
#endif
static_assert(PAKAR_S3_LED_SWAP_RED_BLUE==0 || PAKAR_S3_LED_SWAP_RED_BLUE==1,"LED channel swap must be boolean");
// Preserve API RGB exactly by default. The former implicit TypicalLEDStrip
// attenuation (RGB255/176/240) is available only as an explicit calibrated
// build: -DPAKAR_S3_LED_RGB_GAINS=0xffb0f0. Never silently tint full white.
#ifndef PAKAR_S3_LED_RGB_GAINS
#define PAKAR_S3_LED_RGB_GAINS 0xffffff
#endif
static_assert(PAKAR_S3_LED_RGB_GAINS>=0 && uint64_t(PAKAR_S3_LED_RGB_GAINS)<=0xffffffU,
              "S3 LED RGB gains must be exactly three unsigned8-bit channels");
inline constexpr uint32_t rgbGains=PAKAR_S3_LED_RGB_GAINS;
inline constexpr uint8_t channelGains[]={uint8_t(rgbGains>>16),uint8_t(rgbGains>>8),uint8_t(rgbGains)};
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
// This production output is one complete480 RGB-pixel chain. Keep its actual
// wire budget separate from a client's requested or measured frame rate.
inline constexpr size_t outputPixels=BoardCore::GameRole::pixels,outputBytes=outputPixels*3;
inline constexpr uint32_t dataSymbols=outputPixels*24;
inline constexpr uint32_t rmtResolutionHz=40000000,bitNanos=1250;
inline constexpr uint32_t dataMicros=dataSymbols*bitNanos/1000;
inline constexpr uint32_t minimumFrameMicros=dataMicros+resetMicros;
inline constexpr uint32_t maximumWireFpsMilli=1000000000U/minimumFrameMicros;
// IDF5.5.5 uses exactly two DMA descriptors, each at most4095 bytes.2046
// four-byte symbols give two4092-byte windows: the largest legal native ring.
// A whole480-pixel pre-expanded buffer cannot be substituted for this ring.
inline constexpr size_t dmaRingSymbols=2046;
static_assert((PAKAR_TABLE_SELECTION && outputBytes==1440 && dataSymbols==11520 && dataMicros==14400) ||
              (PAKAR_FIRST_WINS_ENABLED && outputBytes==648 && dataSymbols==5184 && dataMicros==6480),
              "Recheck the WS2811 wire budget if the production table changes");
static_assert(minimumFrameMicros<1000000U/60U,
              "The wire alone must leave room for60 complete480-pixel frames/s");
static_assert(dmaRingSymbols%2==0 && dmaRingSymbols/2*4<=4095,
              "Each native IDF DMA half must fit one hardware descriptor");
inline constexpr uint16_t staticBitCycles=300,staticZeroHighCycles=60,staticOneHighCycles=144;
template<class Rgb>
void encodeWireBytes(const Rgb *rgb,size_t count,uint8_t brightness,bool enabled,uint8_t *out){
  for(size_t p=0;p<count;++p){
    const uint8_t raw[]={rgb[p].r,rgb[p].g,rgb[p].b};
    for(size_t c=0;c<3;++c){
      const size_t logical=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
      out[p*3+c]=enabled?wireChannel(raw[logical],brightness,channelGains[logical]):0;
    }
  }
}
template<class Rgb,class Symbol>
void encodeWire(const Rgb *rgb,size_t count,uint8_t brightness,bool enabled,Symbol *out){
  size_t n=0;
  for(size_t p=0;p<count;++p){
    const uint8_t raw[]={rgb[p].r,rgb[p].g,rgb[p].b};
    for(size_t c=0;c<3;++c){
      const size_t logical=PAKAR_S3_LED_SWAP_RED_BLUE?2-c:c;
      const uint8_t value=enabled?wireChannel(raw[logical],brightness,channelGains[logical]):0;
      for(uint8_t bit=0x80;bit;bit>>=1){auto &s=out[n++];const bool one=value&bit;
        s.level0=1;s.duration0=one?oneHigh:zeroHigh;s.level1=0;s.duration1=one?oneLow:zeroLow;}
    }
  }
  out[n].level0=0;out[n].duration0=resetHalf;out[n].level1=0;out[n].duration1=resetHalf;
}
}
