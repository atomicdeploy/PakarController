#include "../firmware/esp/src/UpdateProgress.h"
#include "../firmware/esp/src/SemanticPanel.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <iostream>
int main() {
  using namespace PakarUpdate;
  uint32_t size=99;
  for(const char *invalid:{"", "0", "-1", "12x", " 12", "4294967296", "65536", "00000000001"})
    assert(!imageSize(invalid,65536,size));
  assert(imageSize("65535",65536,size)&&size==65535);
  Progress progress;char text[5];
  assert(!progress.snapshot().known());progress.begin(0);progress.advance(123);
  panelText(progress.snapshot(),text);assert(!strcmp(text,"UPdt"));
  progress.begin(480);progress.advance(240);assert(progress.snapshot().percent()==50);
  panelText(progress.snapshot(),text);assert(!strcmp(text,"P050"));
  progress.advance(480);assert(progress.snapshot().percent()==100);
  progress.phase(Phase::Verifying);panelText(progress.snapshot(),text);assert(!strcmp(text,"CHEC"));
  assert(!progress.snapshot().known()); // Receiving100% is not verified100%.
  progress.phase(Phase::Rebooting);panelText(progress.snapshot(),text);assert(!strcmp(text,"boot"));
  progress.phase(Phase::Failed);panelText(progress.snapshot(),text);assert(!strcmp(text,"Err "));
  Observation overflow{Phase::Receiving,UINT32_MAX,UINT32_MAX};assert(overflow.percent()==100);
  overflow.expected=1;assert(!overflow.known());
  // Concurrent reader never observes fields combined across different begins.
  std::atomic<bool> done{false};
  std::thread writer([&]{for(uint32_t i=1;i<100000;++i)progress.begin(i);done=true;});
  while(!done){const auto value=progress.snapshot();if(value.phase==Phase::Receiving)assert(value.received==0);}
  writer.join();
  progress.begin(480,27);progress.advance(300);
  assert(progress.snapshot().operation==27 && progress.snapshot().received==300);
  progress.phase(Phase::Rebooting);assert(progress.snapshot().operation==27 && !progress.snapshot().known());
  Completion completion;Completion::Intent intent;
  assert(!completion.take(intent));
  completion.publish(27,480,"0123456789abcdef0123456789abcdef");
  assert(completion.take(intent) && intent.operation==27 && intent.bytes==480);
  assert(!strcmp(intent.md5,"0123456789abcdef0123456789abcdef") && !completion.take(intent));
  completion.publish(28,512,"x");assert(completion.take(intent) && !strcmp(intent.md5,"x"));
  Panel panel;uint8_t payload[12];
  assert(!panel.next(0,true,false,"P001",payload));
  assert(panel.next(0,true,true,"P001",payload));
  assert(payload[0]==5&&payload[3]==4&&payload[4]==0&&(payload[5]|payload[6]<<8)==1200);
  assert(!memcmp(payload+8,"P001",4));
  assert(!panel.next(249,true,true,"P002",payload));
  assert(panel.next(250,true,true,"P002",payload));
  assert(!panel.next(1249,true,true,"P002",payload));
  assert(panel.next(1250,true,true,"P002",payload));
  assert(!panel.next(5000,true,false,"P003",payload));
  assert(!panel.next(5000,false,true,"P003",payload));
  assert(panel.next(UINT32_MAX-100,true,true,"P004",payload));
  assert(!panel.next(100,true,true,"P005",payload));
  assert(panel.next(200,true,true,"P005",payload));
  // The integrated owner is now SemanticPanel, shared by updates and ordinary
  // notices. Exercise its actual lease bytes/admission, not only source tokens.
  PakarPanel::Presenter semantic;
  semantic.persistent(true,"P042");
  assert(!semantic.frame(1000,false,false,payload));
  assert(!semantic.frame(1000,true,true,payload));
  assert(semantic.frame(1000,true,false,payload));
  assert(payload[0]==5 && payload[3]==4 && payload[4]==0 &&
      (payload[5]|payload[6]<<8)==1200 && !memcmp(payload+8,"P042",4));
  assert(semantic.frame(1001,true,false,payload)); // Failed send: still eligible.
  semantic.admitted(1001,payload);
  assert(!semantic.frame(1900,true,false,payload));
  assert(semantic.frame(1901,true,false,payload));
  semantic.post(PakarPanel::Fault,"Err ",0,2000,95,PakarPanel::DoubleFast,6000);
  semantic.view(2000,text);assert(!strcmp(text,"P042")); // Active update wins.
  semantic.persistent(false,nullptr);
  semantic.view(2000,text);assert(!strcmp(text,"Err "));
  semantic.view(8000,text);assert(!strcmp(text,"C001")); // Finite, no replay.
  const uint8_t manualLease[]={5,80,0,4,0,0xd0,7,0,'T','E','S','T'};
  semantic.manual(manualLease,sizeof(manualLease),0xfffffff0U);
  assert(semantic.manualActive(0x100U) && !semantic.manualActive(0x800U));
  std::cout<<"Update progress: measured/unknown stages, atomic snapshot, finite/coalesced panel and rollover passed. No hardware proof.\n";
}
