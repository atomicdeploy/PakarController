#include "../firmware/esp8266/src/SemanticPanel.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace PakarPanel;
static void shown(const Presenter &p,uint32_t now,const char *expected) {
  char text[5];p.view(now,text);assert(!strcmp(text,expected));
}
int main() {
  Presenter p;uint8_t wire[48],frame[12];Event event;
  p.resting("C080");shown(p,0,"C080");
  p.post(Cell,"C001",1,1000);shown(p,1000,"C001");shown(p,2999,"C001");shown(p,3000,"C080");
  assert(p.pop(event)&&event.id==1&&event.value==1);
  assert(encode(event,wire)==20&&wire[8]==Cell&&wire[15]==4&&!memcmp(wire+16,"C001",4));
  p.post(State,"SLOW",0,4000,40,Slow);shown(p,4000,"SLOW");shown(p,4500,"    ");shown(p,5000,"SLOW");
  p.post(Fault,"Err ",0,6000,95,DoubleFast,6000);
  shown(p,6000,"Err ");shown(p,6100,"    ");shown(p,6200,"Err ");shown(p,6300,"    ");
  // A normal event is delivered to clients but cannot cover a fault notice.
  p.post(Cell,"C002",2,6400);shown(p,7000,"Err ");shown(p,12000,"C080");
  p.post(Network,"CL DOWN",0,13000,30);
  shown(p,13000,"CL D");shown(p,13630,"L DO");shown(p,14710,"    ");shown(p,15000,"C080");
  assert(!p.frame(16000,false,false,frame));assert(!p.frame(16000,true,true,frame));
  assert(p.frame(16000,true,false,frame)); // Failed sends never consume admission.
  assert(p.frame(16001,true,false,frame));p.admitted(16001,frame);
  assert(!p.frame(16800,true,false,frame));assert(p.frame(16901,true,false,frame));
  p.persistent(true,"P042");shown(p,17000,"P042");p.persistent(false,nullptr);shown(p,17000,"C080");
  const uint8_t capture[]={3,0,0,4,'H','O','S','T'};
  p.manual(capture,sizeof(capture),18000);assert(p.manualActive(0x10000000));
  const uint8_t release[]={4,0,0,0};p.manual(release,4,19000);assert(!p.manualActive(19000));
  const uint8_t scheduled[]={5,80,0,4,0,0xd0,7,0,'T','E','S','T'};
  p.manual(scheduled,sizeof(scheduled),0xfffffff0);assert(p.manualActive(0x100));assert(!p.manualActive(0x800));
  Presenter rollover;rollover.post(Cell,"C003",3,0xfffffff0);shown(rollover,0x100,"C003");shown(rollover,0x800,"C001");
  Presenter overload;
  Presenter longNotice;
  longNotice.post(Cell,"C001 WHITE FADE",1,0);
  assert(longNotice.pop(event));
  assert(event.lifetime==450U+(strlen(event.text)+1U)*180U);
  encode(event,wire);assert(uint16_t(wire[11]|wire[12]<<8)==event.lifetime);
  for(unsigned i=0;i<40;++i)overload.post(Cell,"C001",i,i);
  assert(overload.dropped()==24&&overload.historyCount()==8);
  assert(overload.history(0).id==33&&overload.history(7).id==40);
  assert(overload.pop(event)&&event.id==25);
  unsigned count=1;while(overload.pop(event))++count;assert(count==16);
  std::cout<<"Semantic panel: finite notices, once/blank marquee, priorities, blink phases, manual leases, rollover, bounded events and admission passed.\n";
}
