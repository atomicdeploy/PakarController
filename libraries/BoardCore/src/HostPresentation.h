#pragma once
#include <stdint.h>
#include <string.h>
#include "StateMotion.h"
#include "PixelDiagnostic.h"

namespace BoardCore {
// Host-owned presentation is a compositing layer, never a game-state owner.
// The caller retains the base frame and applies corrections after composition.
template <uint16_t Groups,uint16_t Pixels> class HostPresentation {
 public:
  enum Effect:uint8_t {Solid,Pulse,Ring,Chase,Blink,Breathe,Rainbow,Fade};
  struct Style {
    uint32_t rgb=0;uint16_t periodMs=1000;
    Effect effect=Solid;bool enabled=false,neighbors=true,invert=false;
    void clear(){rgb=0;periodMs=1000;effect=Solid;enabled=false;neighbors=true;invert=false;}
    bool valid()const{return rgb<=0xffffff&&effect<=Fade&&periodMs>=100&&periodMs<=10000;}
  };
  struct Layer {
    Style entries[Groups]{};
    bool enabled=false,overlay=true,physical=false;
    char owner[33]{};
    // In-place reset avoids a second large aggregate temporary and a GCC
    // Xtensa gimplifier ICE on assignment from Layer{} with member defaults.
    void clear(){for(auto &entry:entries)entry.clear();enabled=false;overlay=true;physical=false;memset(owner,0,sizeof(owner));}
  } saved,temporary;
  uint8_t frame[Pixels*3]{};
  bool frameActive=false,suppressed=false;
  uint32_t revision=0,savedRevision=0,lease=0,startedAt=0,temporaryAt=0,savedAt=0;
  uint16_t ttlMs=0;
  bool active()const{return (!suppressed&&saved.enabled)||temporary.enabled;}
  bool animated()const{
    if(!suppressed&&animated(saved))return true;
    return temporary.enabled&&!frameActive&&animated(temporary);
  }
  static bool animated(const Layer &layer){
    if(layer.enabled)for(const auto &s:layer.entries)if(s.enabled&&s.effect!=Solid)return true;
    return false;
  }
  // Publish the terminal fade frame once, then stop requesting animation work.
  // lastPublished is the time of the last *complete* composited frame.
  bool needsFrame(uint32_t now,uint32_t lastPublished)const{
    return (!suppressed&&needsFrame(saved,now-savedAt,lastPublished-savedAt))||
      (temporary.enabled&&!frameActive&&needsFrame(temporary,now-temporaryAt,lastPublished-temporaryAt));
  }
  static bool needsFrame(const Layer &layer,uint32_t age,uint32_t publishedAge){
    if(layer.enabled)for(const auto &s:layer.entries)if(s.enabled&&s.effect!=Solid){
      if(s.effect!=Fade||age<s.periodMs||publishedAge<s.periodMs)return true;
    }
    return false;
  }
  bool service(uint32_t now){
    if(!temporary.enabled||uint32_t(now-startedAt)<ttlMs)return false;
    release();return true;
  }
  void release(){temporary.clear();frameActive=false;lease=0;ttlMs=0;if(revision!=UINT32_MAX)++revision;}
  // A direct scene/reset/diagnostic action regains the display immediately.
  // Durable host settings remain available, but are explicitly suspended until
  // another presentation command (or reboot) rather than silently resurfacing.
  bool retire(){
    if(!active())return false;
    temporary.clear();frameActive=false;lease=0;ttlMs=0;suppressed=true;if(revision!=UINT32_MAX)++revision;return true;
  }
  template<class Rgb,class Layout> void compose(Rgb *out,const Layout &layout,
      const uint8_t *logicalToPhysical,uint32_t now)const{
    if(!suppressed)render(saved,out,layout,logicalToPhysical,now-savedAt);
    if(!temporary.enabled)return;
    if(frameActive){
      for(uint16_t i=0;i<layout.used();++i)out[i]=Rgb(frame[i*3],frame[i*3+1],frame[i*3+2]);
    }else render(temporary,out,layout,logicalToPhysical,now-temporaryAt);
  }
  static const char *name(Effect value){
    switch(value){case Pulse:return "pulse";case Ring:return "ring";case Chase:return "chase";
      case Blink:return "blink";case Breathe:return "breathe";case Rainbow:return "rainbow";case Fade:return "fade";default:return "solid";}
  }
  static bool parse(const char *value,Effect &effect){
    for(uint8_t i=0;i<=Fade;++i)if(value&&!strcmp(value,name(Effect(i)))){effect=Effect(i);return true;}
    return false;
  }
 private:
  template<class Rgb,class Layout> static void render(const Layer &layer,Rgb *out,
      const Layout &layout,const uint8_t *mapping,uint32_t elapsed){
    if(!layer.enabled)return;
    if(!layer.overlay)for(uint16_t i=0;i<Pixels;++i)out[i]=Rgb(0,0,0);
    for(uint16_t id=1;id<=Groups;++id){
      const auto &style=layer.entries[id-1];if(!style.enabled)continue;
      const uint16_t group=layer.physical?id:mapping[id-1];
      if(!group||group>Groups)continue;
      const uint16_t count=layout.count(group),start=layout.start(group);
      for(uint16_t sector=0;sector<count;++sector){
        uint32_t rgb=style.rgb;uint8_t gain=255;
        switch(style.effect){
          case Pulse:gain=StateMotion::pulse(elapsed,style.periodMs);break;
          case Ring:gain=StateMotion::ring(elapsed,style.periodMs,sector,count,style.neighbors,style.invert);break;
          case Chase:gain=StateMotion::chase(elapsed,style.periodMs,start+sector,layout.used());break;
          case Blink:case Breathe:{PixelDiagnostic d;d.periodMs=style.periodMs;
            d.effect=style.effect==Blink?PixelDiagnostic::Blink:PixelDiagnostic::Breathe;gain=d.gain(elapsed);break;}
          case Rainbow:{
            // Compact RGB color wheel; timing is board-local, not host cadence.
            const uint8_t p=uint8_t(uint64_t(elapsed%style.periodMs)*256U/style.periodMs+uint32_t(sector)*256U/count);
            if(p<85)rgb=(uint32_t(255-p*3)<<16)|uint32_t(p*3);
            else if(p<170){const uint8_t q=p-85;rgb=(uint32_t(q*3)<<8)|uint32_t(255-q*3);}
            else{const uint8_t q=p-170;rgb=(uint32_t(q*3)<<16)|(uint32_t(255-q*3)<<8);}break;}
          default:break;
        }
        if(style.effect==Fade){
          const uint16_t alpha=elapsed>=style.periodMs?255U:uint64_t(elapsed)*255U/style.periodMs;
          const auto base=out[start+sector];
          out[start+sector]=Rgb((uint16_t(base.r)*(255U-alpha)+uint16_t((rgb>>16)&255)*alpha+127U)/255U,
            (uint16_t(base.g)*(255U-alpha)+uint16_t((rgb>>8)&255)*alpha+127U)/255U,
            (uint16_t(base.b)*(255U-alpha)+uint16_t(rgb&255)*alpha+127U)/255U);
        }else out[start+sector]=Rgb((uint16_t((rgb>>16)&255)*gain+127U)/255U,
          (uint16_t((rgb>>8)&255)*gain+127U)/255U,(uint16_t(rgb&255)*gain+127U)/255U);
      }
    }
  }
};
}
