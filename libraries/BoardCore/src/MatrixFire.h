#pragma once
#include <stdint.h>
#include <string.h>

namespace BoardCore {
// Fire2012 (Mark Kriegsman, FastLED / MIT) cool / diffuse / ignite / HeatColor
// pipeline, adapted to a rectangular field rather than the strip's cable order.
// See docs/licenses/FastLED-MIT.txt. No heap, delay or IO.
// Rotation changes the flame direction, never cell IDs or correction addresses.
template<uint16_t Width,uint16_t Height> class MatrixFire {
 public:
  static_assert(Width>0&&Height>0&&uint32_t(Width)*Height<=65535,"Invalid fire field dimensions");
  static constexpr uint16_t size=Width*Height;
  static constexpr uint32_t tickMs=16;
  void restart(uint16_t rotation=0) {
    memset(heat_,0,sizeof(heat_));rotation_=rotation<=270&&rotation%90==0?rotation:0;
    started_=false;random_=0x6d2b79f5U;
  }
  void advance(uint32_t now) {
    // Even if command processing was delayed, publish one genuinely cold frame
    // before ignition. Explicitly selecting Fire again restarts this sequence.
    if(!started_){started_=true;at_=now;return;}
    const uint32_t due=uint32_t(now-at_)/tickMs;
    if(!due)return;
    // Bound recovery after stalls: never run an unbounded animation catch-up
    // loop on the network owner. The simulation clock is independent of FPS.
    const uint32_t steps=due>8?8:due;
    for(uint32_t n=0;n<steps;++n)step();
    at_=due>8?now:at_+steps*tickMs;
  }
  uint8_t sample(uint16_t x,uint16_t y)const {
    if(x>=Width||y>=Height)return 0;
    uint16_t u=x,v=Height-1-y;
    switch(rotation_){
      case 90:u=y;v=x;break;
      case 180:u=Width-1-x;v=y;break;
      case 270:u=Height-1-y;v=Width-1-x;break;
    }
    return heat_[v*width()+u];
  }
 private:
  uint16_t width()const{return rotation_==90||rotation_==270?Height:Width;}
  uint16_t height()const{return size/width();}
  uint8_t randomByte(){random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return uint8_t(random_>>24);}
  void step(){
    const uint16_t w=width(),h=height();
    for(auto &value:heat_){const uint8_t cooling=randomByte()%(80U/h+2U);value=value>cooling?value-cooling:0;}
    // Top-down so every sample uses the previous step's lower row. Clamped
    // lateral neighbours make an actual 2-D flame, without edge wraparound.
    for(uint16_t y=h-1;y>0;--y)for(uint16_t x=0;x<w;++x){
      const uint16_t below=(y-1)*w;
      heat_[y*w+x]=uint8_t((2U*heat_[below+x]+heat_[below+(x?x-1:0)]+
        heat_[below+(x+1<w?x+1:x)])/4U);
    }
    // Ignite across the whole base edge, not only the first strip pixels.
    for(uint16_t x=0;x<w;++x)if(randomByte()<180){
      const uint16_t hot=uint16_t(heat_[x])+160U+randomByte()%96U;
      heat_[x]=hot>255?255:hot;
    }
  }
  uint8_t heat_[size]={};
  uint16_t rotation_=0;
  uint32_t at_=0,random_=0x6d2b79f5U;
  bool started_=false;
};
}
