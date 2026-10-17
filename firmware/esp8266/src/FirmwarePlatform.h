#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <esp_app_format.h>
#include <mbedtls/sha256.h>
#include "../../esp32s3/src/LedFlashGuard.h"
#else
#include <Updater.h>
#include <bearssl/bearssl.h>
#endif

// Small platform boundary for the SAME verified transfer state machine. App
// downloads on ESP32 are partition-relative, never raw offset zero (bootloader,
// partition table and private settings must not escape /firmware/download).
namespace FirmwarePlatform {
inline String sketchMd5() {
#if defined(ARDUINO_ARCH_ESP32)
  // Arduino's MD5 loop directly calls ESP.flashRead, bypassing partition APIs.
  PakarS3::ScopedLedFlashPause pause;
  if(!pause) return String();
#endif
  return ESP.getSketchMD5();
}
inline auto flashChipMode() -> decltype(ESP.getFlashChipMode()) {
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::ScopedLedFlashPause pause;
  if(!pause) return FM_UNKNOWN;
#endif
  return ESP.getFlashChipMode();
}
inline uint32_t sketchSize() {
#if defined(ARDUINO_ARCH_ESP32)
  const auto *running=esp_ota_get_running_partition();
  if(!running) return 0;
  const esp_partition_pos_t position{running->address,running->size};
  esp_image_metadata_t metadata{};
  return esp_image_verify(ESP_IMAGE_VERIFY,&position,&metadata)==ESP_OK ? metadata.image_len : 0;
#else
  return ESP.getSketchSize();
#endif
}
inline String updateError() {
#if defined(ARDUINO_ARCH_ESP32)
  return String(Update.errorString());
#else
  return Update.getErrorString();
#endif
}
class Sha256 {
 public:
  Sha256() {
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_sha256_init(&context_);
#endif
    begin();
  }
  ~Sha256() {
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_sha256_free(&context_);
#endif
  }
  Sha256(const Sha256 &other) {
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_sha256_init(&context_);
    mbedtls_sha256_clone(&context_, &other.context_);
    valid_=other.valid_;
#else
    context_=other.context_;
#endif
  }
  Sha256 &operator=(const Sha256 &other) {
    if(this==&other) return *this;
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_sha256_clone(&context_, &other.context_);
    valid_=other.valid_;
#else
    context_=other.context_;
#endif
    return *this;
  }
  void begin() {
#if defined(ARDUINO_ARCH_ESP32)
    valid_ = mbedtls_sha256_starts(&context_, 0) == 0;
#else
    br_sha256_init(&context_);
#endif
  }
  void add(const void *data, size_t size) {
#if defined(ARDUINO_ARCH_ESP32)
    valid_ = valid_ && mbedtls_sha256_update(&context_, static_cast<const uint8_t *>(data), size) == 0;
#else
    br_sha256_update(&context_, data, size);
#endif
  }
  String hex() const {
    uint8_t bytes[32];
#if defined(ARDUINO_ARCH_ESP32)
    if (!valid_) return String();
    mbedtls_sha256_context copy;
    mbedtls_sha256_init(&copy);
    mbedtls_sha256_clone(&copy, &context_);
    const bool ok = mbedtls_sha256_finish(&copy, bytes) == 0;
    mbedtls_sha256_free(&copy);
    if (!ok) return String();
#else
    br_sha256_out(&context_, bytes);
#endif
    char result[65];
    for (unsigned i=0;i<32;++i) snprintf(result+i*2,3,"%02x",bytes[i]);
    return String(result);
  }
 private:
#if defined(ARDUINO_ARCH_ESP32)
  mbedtls_sha256_context context_{};
  bool valid_ = false;
#else
  br_sha256_context context_{};
#endif
};

inline uint32_t otaCapacity() {
#if defined(ARDUINO_ARCH_ESP32)
  const auto *next=esp_ota_get_next_update_partition(nullptr);
  const auto *running=esp_ota_get_running_partition();
  return next && running && next->address!=running->address ? next->size : 0;
#else
  const uint32_t free=ESP.getFreeSketchSpace();
  return free>0x1000 ? (free-0x1000)&0xfffff000 : 0;
#endif
}
inline bool readRunning(uint32_t offset,uint8_t *out,size_t size) {
#if defined(ARDUINO_ARCH_ESP32)
  const auto *running=esp_ota_get_running_partition();
  return out && running && static_cast<uint64_t>(offset)+size<=running->size &&
      esp_partition_read(running,offset,out,size)==ESP_OK;
#else
  return ESP.flashRead(offset,out,(size+3)&~3U);
#endif
}
inline bool runningIntegrity() {
#if defined(ARDUINO_ARCH_ESP32)
  const auto *running=esp_ota_get_running_partition();
  if (!running) return false;
  const esp_partition_pos_t position={running->address,running->size};
  esp_image_metadata_t metadata{};
  return esp_image_verify(ESP_IMAGE_VERIFY,&position,&metadata)==ESP_OK;
#else
  return ESP.checkFlashCRC();
#endif
}
inline uint32_t freeStorage() {
#if defined(ARDUINO_ARCH_ESP32)
  const size_t total=LittleFS.totalBytes(),used=LittleFS.usedBytes();
  return total>=used ? total-used : 0;
#else
  FSInfo info;
  return LittleFS.info(info) && info.totalBytes>=info.usedBytes ? info.totalBytes-info.usedBytes : 0;
#endif
}
inline void abortUpdate() {
#if defined(ARDUINO_ARCH_ESP32)
  Update.abort(); // end(false) CAN COMMIT a complete stream on ESP32.
#else
  Update.end(false);
#endif
}
inline const char *profile() {
#if defined(ARDUINO_ARCH_ESP32)
  return "pakar-esp32s3-atmega328p";
#else
  return "pakar-nodemcu-atmega328p";
#endif
}
inline const char *model() {
#if defined(ARDUINO_ARCH_ESP32)
  return "esp32s3";
#else
  return "nodemcuv2";
#endif
}
} // namespace FirmwarePlatform
