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

#ifndef PAKAR_WS_LOOP_CAPACITY
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
// One complete message retains bounded backpressure while leaving room for
// the full public peer reports, network stack and update lifecycle on LX106.
#define PAKAR_WS_LOOP_CAPACITY 1
#else
#define PAKAR_WS_LOOP_CAPACITY 8
#endif
#endif

// Adapt the synchronized request/callback publication discipline of
// AsyncHttpService. Only complete network messages cross into loop ownership;
// never retain a callback-owned payload pointer or run device code under lock.
class LoopMessages {
 public:
  static constexpr uint8_t kCapacity=PAKAR_WS_LOOP_CAPACITY;
  static_assert(PAKAR_WS_LOOP_CAPACITY>=1 && PAKAR_WS_LOOP_CAPACITY<=8,
                "WebSocket loop capacity outside 1..8");
  static constexpr size_t kMaximumMessage=4096;
  struct Message { uint32_t client; uint32_t epoch; uint16_t length; uint8_t opcode; uint8_t data[kMaximumMessage]; };
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
    if(!data || length>kMaximumMessage) return false;
    Guard lock(mutex_);
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
    if(dispatching_){++rejected_;return false;}
#endif
    if(!ready()||count_==kCapacity){++rejected_;return false;}
    auto &m=items_[(head_+count_)%kCapacity];m.client=client;m.epoch=epoch;m.opcode=opcode;m.length=length;
    memcpy(m.data,data,length);++count_;return true;
  }
  bool pop(Message &result) {
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
    if(dispatching_)return false;
#endif
    Guard lock(mutex_);if(!ready()||!count_)return false;
    result=items_[head_];head_=(head_+1)%kCapacity;--count_;return true;
  }
  void clear() {
    Guard lock(mutex_);
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
    if(dispatching_){clearPending_=true;return;}
#endif
    head_=count_=0;
  }
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  // LX106 dispatch/callbacks cooperate on one owner. Retain the immutable slot
  // across handler yields instead of reserving a second4KB BSS workspace.
  template<class Consumer> bool consume(const Consumer &consumer) {
    if(dispatching_||!count_)return false;
    dispatching_=true;
    struct Release {
      LoopMessages &owner;
      ~Release(){
        if(owner.clearPending_){owner.head_=owner.count_=0;owner.clearPending_=false;}
        else{owner.head_=(owner.head_+1)%kCapacity;--owner.count_;}
        owner.dispatching_=false;
      }
    } release{*this};
    consumer(static_cast<const Message&>(items_[head_]));return true;
  }
#endif
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
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  bool dispatching_=false,clearPending_=false;
#endif
};
