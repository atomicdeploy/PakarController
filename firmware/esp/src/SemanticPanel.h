#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// PCController's finite host lease, label dwell and one-pass/terminal-blank
// marquee adapted to the ESP loop owner. No allocation, flash writes, SDK IO,
// timers or UART in producers; observed events and display admission differ.
namespace PakarPanel {
enum Kind : uint8_t { Rest, Cell, Mode, Effect, Pixel, Network, Progress,
  Fault, Complete, Output, Menu, Scan, State };
enum Blink : uint8_t { Steady, Slow, DoubleFast };
inline const char *name(Kind kind) {
  const char *names[]={"rest","cell","mode","effect","pixel","network","progress",
    "fault","complete","output","menu","scan","state"};
  return kind<=State?names[kind]:"state";
}
inline const char *name(Blink blink) {
  return blink==Slow?"slow":blink==DoubleFast?"double-fast":"steady";
}
struct Event {
  uint32_t id=0, at=0;
  Kind kind=Rest;
  Blink blink=Steady;
  uint8_t priority=0;
  uint16_t lifetime=2000, value=0;
  char text[33]{};
};
class Presenter {
 public:
  void post(Kind kind,const char *text,uint16_t value,uint32_t now,
            uint8_t priority=40,Blink blink=Steady,uint16_t lifetime=2000) {
    Event event;event.id=++revision_;event.at=now;event.kind=kind;event.blink=blink;
    event.priority=priority;event.lifetime=lifetime;event.value=value;
    if(text)for(unsigned i=0;i<32 && text[i];++i)
      event.text[i]=text[i]>=32&&text[i]<=126?text[i]:'?';
    // Advertise the actual one-pass duration to BOTH clients/transports, not
    // a two-second timeout while the physical marquee is still running.
    const auto length=strlen(event.text);
    const uint16_t scroll=length>4?450U+(length+1U)*180U:0;
    if(event.lifetime && scroll>event.lifetime)event.lifetime=scroll;
    // The display coalesces, but clients retain distinct event IDs. A bounded
    // overload discards oldest telemetry, never replays commands or blocks IO.
    if(count_==capacity){head_=(head_+1)%capacity;--count_;++dropped_;}
    queue_[(head_+count_)%capacity]=event;++count_;
    history_[historyNext_]=event;historyNext_=(historyNext_+1)%historyCapacity;
    if(historyCount_<historyCapacity)++historyCount_;
    if(!notice_ || expired(now) || priority>=noticeEvent_.priority) {
      noticeEvent_=event;notice_=lifetime!=0;noticeAt_=now;
    }
  }
  bool pop(Event &event) {
    if(!count_)return false;
    event=queue_[head_];head_=(head_+1)%capacity;--count_;return true;
  }
  void resting(const char *text) { copy4(rest_,text); }
  void restingSegments(bool active,const uint8_t *segments) {
    restRaw_=active;if(active)memcpy(restSegments_,segments,4);
  }
  void persistent(bool active,const char *text) {
    persistent_=active;if(active)copy4(persistentText_,text);
  }
  void view(uint32_t now,char out[5]) const {
    if(persistent_){copy4(out,persistentText_);return;}
    if(!notice_ || expired(now)){copy4(out,rest_);return;}
    const uint32_t elapsed=now-noticeAt_;
    const uint32_t phase=elapsed%1000U;
    const bool lit=noticeEvent_.blink==Steady ||
      (noticeEvent_.blink==Slow?phase<500U:phase<100U || (phase>=200U&&phase<300U));
    if(!lit){copy4(out,"    ");return;}
    const unsigned length=strlen(noticeEvent_.text);
    // Four-character codes stay still. Long notices walk to one blank window
    // ONCE, then settle back to the resting value; never an endless marquee.
    const unsigned offset=length>4?(elapsed<450U?0:(elapsed-450U)/180U):0;
    if(length>4 && offset>length){copy4(out,rest_);return;}
    for(unsigned i=0;i<4;++i)out[i]=offset+i<length?noticeEvent_.text[offset+i]:' ';
    out[4]=0;
  }
  // Renew the old AVR's finite lease, not its permanent capture. Transport
  // failure does not advance the admission clock, and never proves rendering.
  bool frame(uint32_t now,bool available,bool inhibited,uint8_t payload[12]) {
    if(!available||inhibited){sent_=false;return false;}
    char text[5];view(now,text);
    const bool raw=restRaw_&&!persistent_&&(!notice_||expired(now));
    if(raw)memcpy(text,restSegments_,4);
    if(sent_ && (raw==lastRaw_&&!memcmp(text,last_,4)?uint32_t(now-sentAt_)<900U:
        uint32_t(now-sentAt_)<60U))return false;
    const uint8_t prefix[8]={5,4,1,4,0,0xb0,4,0};
    memcpy(payload,prefix,8);if(raw)payload[4]|=0x20;
    memcpy(payload+8,text,4);return true;
  }
  void admitted(uint32_t now,const uint8_t payload[12]) {
    sent_=true;sentAt_=now;lastRaw_=(payload[4]&0x20)!=0;memcpy(last_,payload+8,4);
  }
  void manual(const uint8_t *p,uint8_t n,uint32_t now) {
    if(!p)return;
    if(n>=4 && p[0]==4){manual_=false;return;}
    uint32_t duration=3000;
    if(n>=4) {
      const bool scheduled=p[0]==5 && n>=8;
      const uint8_t flags=scheduled?p[4]:0;
      const uint16_t hold=scheduled?uint16_t(p[5]|p[6]<<8):uint16_t(p[1]|p[2]<<8);
      manualInfinite_=p[0]==3 || (flags&3)!=0 || (!hold&&p[3]<=4);
      duration=p[3]>4?uint32_t(p[3]+1)*(uint16_t(p[1]|p[2]<<8)?uint16_t(p[1]|p[2]<<8):260):hold;
    } else manualInfinite_=false;
    manual_=true;manualAt_=now;manualDuration_=duration;
  }
  bool manualActive(uint32_t now)const {
    return manual_ && (manualInfinite_ || uint32_t(now-manualAt_)<manualDuration_);
  }
  uint32_t revision()const{return revision_;}
  uint32_t dropped()const{return dropped_;}
  unsigned historyCount()const{return historyCount_;}
  const Event &history(unsigned index)const {
    return history_[(historyNext_+historyCapacity-historyCount_+index)%historyCapacity];
  }
  const Event *notice(uint32_t now)const{return notice_&&!expired(now)?&noticeEvent_:nullptr;}
 private:
  bool expired(uint32_t now)const {
    const uint32_t scroll=strlen(noticeEvent_.text)>4?
      450U+(strlen(noticeEvent_.text)+1U)*180U:0;
    const uint32_t duration=scroll>noticeEvent_.lifetime?scroll:noticeEvent_.lifetime;
    return uint32_t(now-noticeAt_)>=duration;
  }
  static void copy4(char *out,const char *text) {
    bool ended=!text;for(unsigned i=0;i<4;++i){const char c=ended?0:text[i];ended=ended||!c;out[i]=c?c:' ';}out[4]=0;
  }
  static constexpr unsigned capacity=16,historyCapacity=8;
  Event queue_[capacity]{},history_[historyCapacity]{},noticeEvent_{};
  unsigned head_=0,count_=0,historyNext_=0,historyCount_=0;
  uint32_t revision_=0,dropped_=0,noticeAt_=0,sentAt_=0;
  bool notice_=false,persistent_=false,sent_=false;
  bool restRaw_=false,lastRaw_=false;
  uint8_t restSegments_[4]{};
  bool manual_=false,manualInfinite_=false;
  uint32_t manualAt_=0,manualDuration_=0;
  char rest_[5]="C001",persistentText_[5]{},last_[4]{};
};
// Shared binary USB event, a single ordinary COBS frame (<=48-byte payload).
inline uint8_t encode(const Event &event,uint8_t out[48]) {
  for(unsigned i=0;i<4;++i){out[i]=event.id>>(i*8);out[4+i]=event.at>>(i*8);}
  out[8]=event.kind;out[9]=event.blink;out[10]=event.priority;
  out[11]=event.lifetime;out[12]=event.lifetime>>8;
  out[13]=event.value;out[14]=event.value>>8;
  out[15]=strlen(event.text);memcpy(out+16,event.text,out[15]);return 16+out[15];
}
} // namespace PakarPanel
