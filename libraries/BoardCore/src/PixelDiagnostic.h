#pragma once
#include <stdint.h>
#include <string.h>

namespace BoardCore {
// One bounded diagnostic layer, composited by the existing frame owner. It
// never owns a driver, saved scene, network timer or flash write.
struct PixelDiagnostic {
  enum Effect : uint8_t { Solid, Blink, Breathe, Rainbow };
  enum Composite : uint8_t { Clear, Overlay };
  Effect effect=Solid;
  Composite composite=Clear;
  uint16_t periodMs=1000;
  uint8_t dutyPercent=50,minBrightness=0,maxBrightness=255;
  bool enabled=true;
  bool valid()const {
    return effect<=Rainbow && composite<=Overlay && periodMs>=100 && periodMs<=10000 &&
      dutyPercent>=1 && dutyPercent<=99 && minBrightness<=maxBrightness;
  }
  bool animated()const{return enabled && effect!=Solid;}
  static const char *name(Effect e){return e==Blink?"blink":e==Breathe?"breathe":e==Rainbow?"rainbow":"solid";}
  static const char *name(Composite c){return c==Overlay?"overlay":"clear";}
  static bool parse(const char *s,Effect &e){
    for(uint8_t i=0;i<=Rainbow;++i)if(s && !strcmp(s,name(Effect(i)))){e=Effect(i);return true;}
    return false;
  }
  static bool parse(const char *s,Composite &c){
    for(uint8_t i=0;i<=Overlay;++i)if(s && !strcmp(s,name(Composite(i)))){c=Composite(i);return true;}
    return false;
  }
  uint8_t gain(uint32_t elapsed)const {
    if(effect==Solid || effect==Rainbow)return maxBrightness;
    const uint32_t phase=elapsed%periodMs;
    if(effect==Blink)return phase*100U<uint32_t(periodMs)*dutyPercent?maxBrightness:minBrightness;
    // Smoothstep triangular wave: zero slope at both ends, integer-only and
    // time based, so network stalls skip old frames instead of slowing motion.
    const uint32_t half=periodMs/2;
    const uint32_t x=phase<half?phase*255U/half:(periodMs-phase)*255U/(periodMs-half);
    const uint32_t smooth=x*x*(765U-2U*x)/65025U;
    return uint8_t(minBrightness+(uint32_t(maxBrightness-minBrightness)*smooth+127U)/255U);
  }
};
}
