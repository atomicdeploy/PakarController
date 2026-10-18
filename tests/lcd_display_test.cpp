#include <LcdDisplay.h>
#include <cassert>
#include <cstdio>
#include <vector>
#include <array>
struct Bus:BoardCore::LcdDisplay::Bus {
  uint8_t address=0x27;bool connected=true;
  unsigned writes=0,failWrite=0;std::vector<uint8_t> probes,values;
  bool probe(uint8_t a)override{probes.push_back(a);return connected&&a==address;}
  bool write(uint8_t a,uint8_t v)override{
    assert(a==address);++writes;values.push_back(v);return connected&&writes!=failWrite;
  }
};
static void advance(BoardCore::LcdDisplay &lcd,uint32_t &now,unsigned ticks=150) {
  for(unsigned i=0;i<ticks;++i){lcd.service(now);now+=1000U;}
}
int main(){
  {Bus bus;BoardCore::LcdDisplay lcd(bus);uint32_t now=0;
    assert(!lcd.detectionKnown()&&!lcd.available());lcd.setText("ledboard","Cell 1 white");lcd.begin(now);
    lcd.service(49999);assert(bus.probes.empty());advance(lcd,now);
    assert(lcd.detectionKnown()&&lcd.detected()&&lcd.initialized()&&lcd.available());
    assert(lcd.address()==0x27&&!lcd.pending()&&lcd.presentedRevision()==lcd.revision());
    assert(!memcmp(lcd.line(0),"ledboard        ",16));
    // Exact donor backpack reset nibbles, with every enable edge/backlight.
    const std::array<uint8_t,12> wake={0x38,0x3c,0x38,0x38,0x3c,0x38,0x38,0x3c,0x38,0x28,0x2c,0x28};
    assert(std::equal(wake.begin(),wake.end(),bus.values.begin()));
    const auto writes=bus.writes;lcd.setText("ledboard","Cell 1 white");advance(lcd,now);
    assert(bus.writes==writes); // Cached unchanged content does not redraw.
    lcd.setText("a","b");assert(lcd.pending());advance(lcd,now);
    assert(!memcmp(lcd.line(0),"a               ",16)&&!lcd.pending());
    bus.connected=false;advance(lcd,now,1200);assert(!lcd.available()&&!lcd.detected());
    bus.connected=true;advance(lcd,now,2200);assert(lcd.available()&&!lcd.pending());
    assert(!memcmp(lcd.line(0),"a               ",16));
    lcd.rescan(now);assert(!lcd.detectionKnown()&&!lcd.available());advance(lcd,now);assert(lcd.available());
  }
  {Bus bus;bus.address=0x3f;BoardCore::LcdDisplay lcd(bus);uint32_t now=UINT32_MAX-70000U;
    lcd.begin(now);lcd.setText("0123456789abcdefghijkl","short");advance(lcd,now,200);
    assert(lcd.available()&&lcd.address()==0x3f&&bus.probes[0]==0x27&&bus.probes[1]==0x3f);
    assert(!memcmp(lcd.line(0),"0123456789abcdef",16));
  }
  {Bus bus;bus.connected=false;BoardCore::LcdDisplay lcd(bus);uint32_t now=0;lcd.begin(now);advance(lcd,now);
    assert(lcd.detectionKnown()&&!lcd.available()&&bus.writes==0&&bus.probes.size()==2);
    advance(lcd,now,500);assert(bus.probes.size()==2); // Bounded retry, not a scan storm.
  }
  // Every reset/init/character write position can fail. No subsequent ACK
  // may turn a partially initialized/presented image into a successful display.
  for(unsigned position=1;position<=246;++position){
    Bus bus;bus.failWrite=position;BoardCore::LcdDisplay lcd(bus);uint32_t now=0;lcd.begin(now);advance(lcd,now);
    if(position<=246){assert(!lcd.available()&&lcd.error()!=nullptr);}
    bus.failWrite=0;advance(lcd,now,2200);assert(lcd.available());
  }
  puts("LCD donor transport: bounded scan/init, checked writes, cache, recovery and rollover passed using bus doubles; not optical/electrical proof.");
}
