#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#include <new>
#endif
#if !defined(ESP8266) && !defined(ARDUINO_ARCH_ESP8266)
#include <mutex>
#endif

// Adapt the synchronized request/callback publication discipline of
// AsyncHttpService. Only complete network messages cross into loop ownership;
// never retain a callback-owned payload pointer or run device code under lock.
class LoopMessages {
 public:
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  // Eight full network messages exceed LX106's static DRAM budget with the
  // integrated update lifecycle. Keep explicit backpressure, not heap overflow.
  static constexpr uint8_t kCapacity=4;
#else
  static constexpr uint8_t kCapacity=8;
#endif
  struct Message { uint32_t client; uint32_t epoch; uint16_t length; uint8_t opcode; uint8_t data[1300]; };
  LoopMessages()=default;
  LoopMessages(const LoopMessages&)=delete;
  LoopMessages& operator=(const LoopMessages&)=delete;
#if defined(ARDUINO_ARCH_ESP32)
  ~LoopMessages(){if(items_)heap_caps_free(items_);}
#endif
  // Setup owns the only allocation, before installing network callbacks. The
  // SDK's explicit PSRAM allocator has no internal-memory fallback. Never
  // allocate under a callback lock or borrow the Wi-Fi/OTA internal heap.
  bool begin() {
#if defined(ARDUINO_ARCH_ESP32)
    if(items_)return true;
    void *memory=heap_caps_malloc(sizeof(Message)*kCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!memory)return false;
    items_=static_cast<Message*>(memory);
    for(uint8_t i=0;i<kCapacity;++i)new(items_+i) Message{};
#endif
    return true;
  }
  bool ready() const {
#if defined(ARDUINO_ARCH_ESP32)
    return items_!=nullptr;
#else
    return true;
#endif
  }
  bool push(uint32_t client,uint8_t opcode,const uint8_t *data,size_t length,uint32_t epoch=0) {
    if(!data || length>1300) return false;
    Guard lock(mutex_);
    if(!ready()||count_==kCapacity){++rejected_;return false;}
    auto &m=items_[(head_+count_)%kCapacity];m.client=client;m.epoch=epoch;m.opcode=opcode;m.length=length;
    memcpy(m.data,data,length);++count_;return true;
  }
  bool pop(Message &result) {
    Guard lock(mutex_);if(!ready()||!count_)return false;
    result=items_[head_];head_=(head_+1)%kCapacity;--count_;return true;
  }
  uint32_t rejected(){Guard lock(mutex_);return rejected_;}
 private:
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  struct Mutex {void lock(){} void unlock(){}};
#else
  using Mutex=std::mutex;
#endif
  struct Guard {Mutex &m;explicit Guard(Mutex &v):m(v){m.lock();}~Guard(){m.unlock();}};
  Mutex mutex_;
#if defined(ARDUINO_ARCH_ESP32)
  Message *items_=nullptr;
#else
  Message items_[kCapacity]{};
#endif
  uint8_t head_=0,count_=0;uint32_t rejected_=0;
};
