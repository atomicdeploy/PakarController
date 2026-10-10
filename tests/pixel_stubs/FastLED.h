#pragma once
#include "Arduino.h"
struct CRGB {
 uint8_t r=0,g=0,b=0;
 CRGB()=default; CRGB(uint8_t red,uint8_t green,uint8_t blue):r(red),g(green),b(blue){}
 bool operator==(const CRGB& o)const{return r==o.r&&g==o.g&&b==o.b;}
 bool operator!=(const CRGB& o)const{return !(*this==o);}
 static const CRGB White,Red,Black;
};
inline const CRGB CRGB::White{255,255,255},CRGB::Red{255,0,0},CRGB::Black{0,0,0};
struct WS2811{};
constexpr int RGB=0,TypicalLEDStrip=0;
struct TestFastLED {
 CRGB* pixels=nullptr;unsigned shows=0;
 template<class Chip,uint8_t Pin,int Order>TestFastLED& addLeds(CRGB* p,uint16_t){pixels=p;return *this;}
 TestFastLED& setCorrection(int){return *this;}
 void setBrightness(uint8_t){}
 void show(){++shows;}
};
inline TestFastLED FastLED;
inline void fill_solid(CRGB* p,uint16_t n,CRGB c){for(uint16_t i=0;i<n;++i)p[i]=c;}
inline void fill_rainbow(CRGB* p,uint16_t n,uint8_t hue,uint8_t step){for(uint16_t i=0;i<n;++i)p[i]=CRGB(static_cast<uint8_t>(hue+i*step),20,30);}
inline uint8_t qsub8(uint8_t a,uint8_t b){return a>b?a-b:0;}
inline uint8_t qadd8(uint8_t a,uint8_t b){return min<int>(255,a+b);}
inline uint8_t random8(uint8_t=255,uint8_t=255){return 0;}
inline CRGB HeatColor(uint8_t n){return CRGB(n,0,0);}
