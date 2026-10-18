#pragma once
#include <stdint.h>
#include <string.h>

// Adapted from PCController LocalLib/I2cLcd.h/.cpp (MIT). Same common
// PCF8574 wiring and bounded 0x27/0x3F discovery, but every write is checked.
// One LCD byte per service call; initialization waits never block the loop.
// A backpack ACK is not proof of visible glass, contrast or correct wiring.
namespace BoardCore {
class LcdDisplay {
 public:
  struct Bus {
    virtual ~Bus() = default;
    virtual bool probe(uint8_t address) = 0;
    virtual bool write(uint8_t address, uint8_t value) = 0;
  };
  explicit LcdDisplay(Bus &bus) : bus_(bus) {
    memset(desired_, ' ', sizeof(desired_));
    memset(committed_, ' ', sizeof(committed_));
  }
  void begin(uint32_t now) { begun_=true;rescan(now); }
  void rescan(uint32_t now) {
    address_=0;initialized_=haveFrame_=known_=false;candidate_=0;phase_=Scan;
    next_=now+50000U;error_=nullptr; // HD44780 power-on settling.
  }
  bool detected() const { return address_!=0; }
  bool initialized() const { return initialized_; }
  bool available() const { return detected()&&initialized_&&haveFrame_; }
  bool detectionKnown() const { return known_; }
  uint8_t address() const { return address_; }
  uint32_t revision() const { return revision_; }
  uint32_t presentedRevision() const { return presented_; }
  bool pending() const { return !available()||presented_!=revision_; }
  const char *error() const { return error_; }
  const char *line(uint8_t row) const { return row<2 ? committed_+row*16 : nullptr; }
  bool backlight() const { return backlight_; }
  void setBacklight(bool enabled) {
    if(backlight_!=enabled){backlight_=enabled;controlDirty_=true;}
  }
  void setText(const char *one,const char *two) {
    char normalized[32];normalize(one,normalized);normalize(two,normalized+16);
    if(!memcmp(normalized,desired_,32))return;
    memcpy(desired_,normalized,32);++revision_;
  }
  void service(uint32_t now) {
    if(!begun_||static_cast<int32_t>(now-next_)<0)return;
    switch(phase_) {
      case Scan: {
        const uint8_t address=candidate_++==0?0x27:0x3f;
        if(bus_.probe(address)) {address_=address;known_=true;phase_=Wake;step_=0;}
        else if(candidate_==2) {known_=true;fail(now,"No supported LCD backpack ACK");}
        else next_=now+10000U;
        break;
      }
      case Wake:
        if(!nibble(step_<3?0x30:0x20)){fail(now,"LCD wake write failed");break;}
        next_=now+(step_<2?5000U:1000U);
        if(++step_==4){phase_=Initialize;step_=0;}
        break;
      case Initialize: {
        static const uint8_t commands[]={0x28,0x08,0x01,0x06,0x0c};
        if(!send(commands[step_],false)){fail(now,"LCD initialization write failed");break;}
        // At100kHz a full byte takes about1ms. Allow >2ms AFTER clear's
        // final strobe as well, not merely after this call's start time.
        next_=now+(step_==2?4000U:0U);
        if(++step_==sizeof(commands)){initialized_=true;startFrame();}
        break;
      }
      case Render:
        if(column_==0xff) {
          if(!send(row_?0xc0:0x80,false)){fail(now,"LCD row address failed");break;}
          column_=0;
        } else {
          if(!send(static_cast<uint8_t>(frame_[row_*16+column_]),true)) {
            fail(now,"LCD character write failed");break;
          }
          if(++column_==16) {
            if(++row_==2) {
              memcpy(committed_,frame_,32);presented_=frameRevision_;
              haveFrame_=true;phase_=Ready;healthAt_=now;error_=nullptr;
            }else column_=0xff;
          }
        }
        break;
      case Ready:
        if(controlDirty_) {
          if(!expander(0)){fail(now,"LCD backlight write failed");break;}
          controlDirty_=false;
        }else if(presented_!=revision_)startFrame();
        else if(static_cast<uint32_t>(now-healthAt_)>=1000000U) {
          healthAt_=now;
          if(!bus_.probe(address_))fail(now,"LCD backpack disconnected");
        }
        break;
      case Retry: candidate_=0;phase_=Scan;break;
    }
  }
 private:
  enum Phase:uint8_t {Scan,Wake,Initialize,Render,Ready,Retry};
  static void normalize(const char *source,char *destination) {
    uint8_t i=0;
    if(source)for(;i<16&&source[i];++i) {
      const auto c=static_cast<uint8_t>(source[i]);
      destination[i]=c>=0x20&&c<=0x7e?static_cast<char>(c):'?';
    }
    while(i<16)destination[i++]=' ';
  }
  bool expander(uint8_t value) {
    return bus_.write(address_,static_cast<uint8_t>(value|(backlight_?0x08:0)));
  }
  bool nibble(uint8_t value) {
    // At the adapter's fixed100kHz, each expander transaction is slower than
    // the1us enable pulse and50us ordinary instruction delay of the donor.
    return expander(value)&&expander(value|0x04)&&expander(value&~0x04);
  }
  bool send(uint8_t value,bool data) {
    const uint8_t rs=data?1:0;
    return nibble((value&0xf0)|rs)&&nibble(static_cast<uint8_t>(value<<4)|rs);
  }
  void startFrame() {
    memcpy(frame_,desired_,32);frameRevision_=revision_;row_=0;column_=0xff;phase_=Render;
  }
  void fail(uint32_t now,const char *message) {
    address_=0;initialized_=haveFrame_=false;known_=true;error_=message;
    phase_=Retry;next_=now+2000000U;
  }
  Bus &bus_;
  char desired_[32],frame_[32],committed_[32];
  uint32_t next_=0,healthAt_=0,revision_=0,presented_=0,frameRevision_=0;
  uint8_t candidate_=0,address_=0,step_=0,row_=0,column_=0;
  Phase phase_=Scan;
  bool begun_=false,known_=false,initialized_=false,haveFrame_=false;
  bool backlight_=true,controlDirty_=false;
  const char *error_=nullptr;
};
}
