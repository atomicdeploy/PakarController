#pragma once
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <stdint.h>

// The inactive OTA slot is the ESP image staging area, not LittleFS. Finalizing
// an image validates it but NEVER selects it for boot. Selection is a separate
// explicit, journaled apply operation after the caller's SHA/header checks.
class S3OtaStage {
 public:
  ~S3OtaStage() { abort(); }
  bool begin(uint32_t bytes) {
    abort(); partition_=nullptr;size_=written_=0;ready_=false;
    const auto *running=esp_ota_get_running_partition();
    const auto *boot=esp_ota_get_boot_partition();
    const auto *next=esp_ota_get_next_update_partition(nullptr);
    if(!running || !boot || boot->address!=running->address || !next ||
       next->address==running->address || bytes<24 || bytes>=next->size) return false;
    if(esp_ota_begin(next,OTA_WITH_SEQUENTIAL_WRITES,&handle_)!=ESP_OK) return false;
    partition_=next;size_=bytes;active_=true;return true;
  }
  bool write(const uint8_t *data,size_t bytes) {
    if(!active_ || !data || written_>size_ || bytes>size_-written_ ||
       esp_ota_write(handle_,data,bytes)!=ESP_OK) return false;
    written_+=bytes;return true;
  }
  bool finish() {
    if(!active_ || written_!=size_) {abort();return false;}
    active_=false; // SDK consumes the handle even when esp_ota_end fails.
    ready_=esp_ota_end(handle_)==ESP_OK;return ready_;
  }
  void abort() {
    if(active_) {esp_ota_abort(handle_);active_=false;ready_=false;}
  }
  bool resume(uint32_t address,uint32_t bytes) {
    abort();ready_=false;partition_=nullptr;size_=written_=0;
    const auto *next=esp_ota_get_next_update_partition(nullptr);
    const auto *running=esp_ota_get_running_partition();
    if(!next || !running || next->address==running->address || next->address!=address ||
       bytes<24 || bytes>=next->size) return false;
    const esp_partition_pos_t pos{next->address,next->size};esp_image_metadata_t metadata{};
    if(esp_image_verify(ESP_IMAGE_VERIFY,&pos,&metadata)!=ESP_OK || metadata.image_len!=bytes) return false;
    partition_=next;size_=written_=bytes;ready_=true;return true;
  }
  bool read(uint32_t offset,uint8_t *data,size_t bytes) const {
    return ready_ && partition_ && data && static_cast<uint64_t>(offset)+bytes<=size_ &&
        esp_partition_read(partition_,offset,data,bytes)==ESP_OK;
  }
  bool selectForBoot() {
    const auto *running=esp_ota_get_running_partition();
    const auto *next=esp_ota_get_next_update_partition(nullptr);
    const auto *boot=esp_ota_get_boot_partition();
    return ready_ && partition_ && next && running && boot &&
        boot->address==running->address && partition_->address==next->address &&
        partition_->address!=running->address && esp_ota_set_boot_partition(partition_)==ESP_OK;
  }
  uint32_t address() const {return ready_ && partition_?partition_->address:0;}
  uint32_t size() const {return ready_?size_:0;}
 private:
  const esp_partition_t *partition_=nullptr;
  esp_ota_handle_t handle_=0;
  uint32_t size_=0,written_=0;
  bool active_=false,ready_=false;
};
