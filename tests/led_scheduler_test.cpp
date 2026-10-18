#include "LoopMessages.h"
#include "../firmware/esp32s3/src/LedWire.h"
#include <cassert>
#include <thread>
#include <atomic>
#include <cstdio>
int main(){
  using namespace PakarS3;
  assert(zeroHigh+zeroLow==50 && oneHigh+oneLow==50 && resetHalf*2==12000);
  assert(wireChannel(255,255,255)==255 && wireChannel(255,255,176)==176);
  assert(wireChannel(255,0,255)==0 && wireChannel(255,128,255)==128);
  struct Rgb{uint8_t r,g,b;};struct Symbol{uint16_t level0,duration0,level1,duration1;};
  Rgb rgb[480];Symbol symbols[480*24+1];memset(rgb,255,sizeof(rgb));memset(symbols,255,sizeof(symbols));
  encodeWire(rgb,480,255,false,symbols);
  for(size_t i=0;i<480*24;++i)assert(symbols[i].level0==1&&symbols[i].duration0==zeroHigh&&symbols[i].duration1==zeroLow&&symbols[i].level1==0);
  assert(symbols[480*24].level0==0&&symbols[480*24].duration0==resetHalf);
  rgb[0]={255,0,0};rgb[479]={0,0,255};encodeWire(rgb,480,255,true,symbols);
  for(size_t i=0;i<16;++i)assert(symbols[i].duration0==zeroHigh);
  for(size_t i=16;i<24;++i)assert(symbols[i].duration0==oneHigh);
  assert(symbols[479*24].duration0==oneHigh); // BGR wire order, logical blue correction MSB.
  LoopMessages queue;uint8_t bytes[]={1,2,3};LoopMessages::Message message;
#if defined(ARDUINO_ARCH_ESP32)
  static_assert(sizeof(LoopMessages)<512,"S3 queue must not retain internal bulk storage");
  assert(!queue.ready()&&!queue.pop(message));
  PakarPsramFixture::fail=true;
  assert(!queue.begin()&&!queue.ready());
  assert(PakarPsramFixture::size==sizeof(message)*LoopMessages::kCapacity);
  PakarPsramFixture::fail=false;
#endif
  assert(queue.begin()&&queue.begin()&&queue.ready());
  assert(!queue.push(1,2,nullptr,3));assert(!queue.push(1,2,bytes,1301));
  for(unsigned i=0;i<LoopMessages::kCapacity;++i){bytes[0]=i;assert(queue.push(i,2,bytes,3));}
  bytes[0]=99;assert(!queue.push(9,2,bytes,3));assert(queue.rejected()==1);
  for(unsigned i=0;i<LoopMessages::kCapacity;++i){assert(queue.pop(message));assert(message.client==i && message.data[0]==i && message.opcode==2 && message.length==3);}
  assert(!queue.pop(message));
#if defined(ARDUINO_ARCH_ESP8266)
  static_assert(LoopMessages::kCapacity==4,"ESP8266 DRAM ingress budget regressed");
  puts("ESP8266 cooperative complete-message FIFO: bounded capacity4, backpressure/copy/order preserved; no hardware timing proof.");
#else
  static_assert(LoopMessages::kCapacity==8,"S3/concurrent ingress capacity changed");
  std::atomic<bool> done{false};std::atomic<unsigned> accepted{0};unsigned popped=0;
  std::thread producer([&]{for(unsigned i=0;i<20000;++i){uint8_t data[4];memcpy(data,&i,4);if(queue.push(i,2,data,4))++accepted;}done.store(true);});
  for(;;){if(queue.pop(message)){unsigned id;memcpy(&id,message.data,4);assert(id==message.client);++popped;}
    else if(done.load())break;else std::this_thread::yield();}
  producer.join();while(queue.pop(message))++popped;assert(popped==accepted);
  puts("Loop-owned complete-message FIFO, bounds/backpressure/copy isolation,20000 cross-thread publications and WS2811 wire scaling/timing passed; not optical/DMA hardware proof.");
#endif
}
