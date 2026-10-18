#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if !defined(ESP8266) && !defined(ARDUINO_ARCH_ESP8266)
#include <mutex>
#endif

// Adapt the synchronized request/callback publication discipline of
// AsyncHttpService. Only complete network messages cross into loop ownership;
// never retain a callback-owned payload pointer or run device code under lock.
class LoopMessages {
 public:
  struct Message { uint32_t client; uint32_t epoch; uint16_t length; uint8_t opcode; uint8_t data[1300]; };
  bool push(uint32_t client,uint8_t opcode,const uint8_t *data,size_t length,uint32_t epoch=0) {
    if(!data || length>1300) return false;
    Guard lock(mutex_);
    if(count_==8){++rejected_;return false;}
    auto &m=items_[(head_+count_)%8];m.client=client;m.epoch=epoch;m.opcode=opcode;m.length=length;
    memcpy(m.data,data,length);++count_;return true;
  }
  bool pop(Message &result) {
    Guard lock(mutex_);if(!count_)return false;
    result=items_[head_];head_=(head_+1)%8;--count_;return true;
  }
  uint32_t rejected(){Guard lock(mutex_);return rejected_;}
 private:
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  struct Mutex {void lock(){} void unlock(){}};
#else
  using Mutex=std::mutex;
#endif
  struct Guard {Mutex &m;explicit Guard(Mutex &v):m(v){m.lock();}~Guard(){m.unlock();}};
  Mutex mutex_;Message items_[8]{};uint8_t head_=0,count_=0;uint32_t rejected_=0;
};
