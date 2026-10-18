#pragma once
#include <BleWire.h>
#include <esp_heap_caps.h>
#include <new>

namespace PakarS3 {
struct BleIngress {
  uint32_t epoch=0;
  uint16_t peer=0xffff,length=0;
  uint8_t bytes[512]{};
};
// Only task-owned application bytes live here. SDK handles, atomics, locks,
// callback objects and LED/cache-sensitive storage remain in internal memory.
// Moving the WHOLE framing objects preserves their wire/receipt behavior and
// removes their embedded arrays from BleService's internal-RAM static object.
struct BleBulkStorage {
  BleIngress ingress[8]{};
  BoardCore::BleWire::Receiver receiver;
  BoardCore::BleWire::Sender sender;
};
class BleBulkOwner {
 public:
  BleBulkOwner()=default;
  BleBulkOwner(const BleBulkOwner&)=delete;
  BleBulkOwner& operator=(const BleBulkOwner&)=delete;
  ~BleBulkOwner() {
    if(value_){value_->~BleBulkStorage();heap_caps_free(value_);}
  }
  // Called after Arduino PSRAM initialization and before BLEDevice::init.
  // One lifetime allocation; fences/disable/reconnect never free or replace it
  // while host callbacks might still be copying an ingress fragment.
  bool begin() {
    if(value_)return true;
    void *memory=heap_caps_malloc(sizeof(BleBulkStorage),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!memory)return false; // No internal-memory fallback that can starve OTA.
    value_=new(memory) BleBulkStorage{};
    return true;
  }
  explicit operator bool() const {return value_!=nullptr;}
  BleBulkStorage *get() const {return value_;}
  BleBulkStorage *operator->() const {return value_;}
 private:
  BleBulkStorage *value_=nullptr;
};
} // namespace PakarS3
