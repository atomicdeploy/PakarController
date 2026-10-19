#include "LoopMessages.h"
#include "../firmware/esp32s3/src/LedWire.h"
#include <cassert>
#include <thread>
#include <atomic>
#include <cstdio>
int main(){
  using namespace PakarS3;
  assert(zeroHigh+zeroLow==50 && oneHigh+oneLow==50 && resetHalf*2==12000);
  assert(zeroHigh==12 && zeroLow==38 && oneHigh==25 && oneLow==25); // WS2811 V1.4:300/950 and625/625ns, away from the previous timing boundaries.
  assert(outputPixels==480 && outputBytes==1440 && dataSymbols==11520);
  assert(dataMicros==14400 && minimumFrameMicros==14700 && maximumWireFpsMilli==68027);
  assert(dmaRingSymbols==2046 && (dmaRingSymbols/2)*4==4092);
  assert(wireChannel(255,255,255)==255 && wireChannel(255,255,176)==176);
  assert(wireChannel(255,0,255)==0 && wireChannel(255,128,255)==128);
  struct Rgb{uint8_t r,g,b;};struct Symbol{uint16_t level0,duration0,level1,duration1;};
  Rgb rgb[480];Symbol symbols[480*24+1];memset(rgb,255,sizeof(rgb));memset(symbols,255,sizeof(symbols));
  static_assert(rgbGains==0xffffff,"Neutral default must preserve full-white API values");
  uint8_t fullBytes[480*3];encodeWireBytes(rgb,480,255,true,fullBytes);
  for(uint8_t value:fullBytes)assert(value==255);
  encodeWire(rgb,480,255,true,symbols);
  for(size_t i=0;i<480*24;++i)assert(symbols[i].duration0==oneHigh&&symbols[i].duration1==oneLow);
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
  assert(!queue.push(1,2,nullptr,3));assert(!queue.push(1,2,bytes,LoopMessages::kMaximumMessage+1));
  uint8_t import[LoopMessages::kMaximumMessage]{};import[4095]=42;
  assert(queue.push(4,2,import,sizeof(import)));assert(queue.pop(message));assert(message.length==4096&&message.data[4095]==42);
  for(unsigned i=0;i<LoopMessages::kCapacity;++i){bytes[0]=i;assert(queue.push(i,2,bytes,3));}
  bytes[0]=99;assert(!queue.push(9,2,bytes,3));assert(queue.rejected()==1);
  for(unsigned i=0;i<LoopMessages::kCapacity;++i){assert(queue.pop(message));assert(message.client==i && message.data[0]==i && message.opcode==2 && message.length==3);}
  assert(!queue.pop(message));
#if defined(ARDUINO_ARCH_ESP8266)
  assert(queue.push(7,2,import,sizeof(import)));bool consumed=false;
  assert(queue.consume([&](const LoopMessages::Message &borrowed){
    consumed=true;assert(borrowed.client==7&&borrowed.data[4095]==42);
    assert(!queue.push(8,2,bytes,sizeof(bytes))&&!queue.pop(message));
    assert(!queue.consume([](const LoopMessages::Message &){}));
    queue.clear();assert(!queue.push(8,2,bytes,sizeof(bytes)));
    assert(borrowed.client==7&&borrowed.data[4095]==42);
  }));
  assert(consumed&&!queue.pop(message));assert(queue.push(8,2,bytes,sizeof(bytes)));
  try{queue.consume([](const LoopMessages::Message &){throw 7;});assert(false);}catch(int value){assert(value==7);}
  assert(queue.push(9,2,bytes,sizeof(bytes)));assert(queue.pop(message));
  static_assert(sizeof(LoopMessages)<=sizeof(LoopMessages::Message)*LoopMessages::kCapacity+16,
                "ESP8266 inline queue RAM budget regressed");
  printf("ESP8266 cooperative complete-message FIFO: bounded capacity%u, backpressure/copy/order preserved; no hardware timing proof.\n",LoopMessages::kCapacity);
#else
  std::atomic<bool> done{false};std::atomic<unsigned> accepted{0};unsigned popped=0;
  std::thread producer([&]{for(unsigned i=0;i<20000;++i){uint8_t data[4];memcpy(data,&i,4);if(queue.push(i,2,data,4))++accepted;}done.store(true);});
  for(;;){if(queue.pop(message)){unsigned id;memcpy(&id,message.data,4);assert(id==message.client);++popped;}
    else if(done.load())break;else std::this_thread::yield();}
  producer.join();while(queue.pop(message))++popped;assert(popped==accepted);
  puts("Loop-owned complete-message FIFO, bounds/backpressure/copy isolation,20000 cross-thread publications and WS2811 wire scaling/timing passed; not optical/DMA hardware proof.");
#endif
}
