#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#if defined(ARDUINO_ARCH_ESP32) && __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_arduino_version.h>
#include <nvs.h>
#else
extern "C" {
#include <osapi.h>
}
#endif

namespace SystemPlatform {
inline bool mountFilesystem() {
#if defined(ARDUINO_ARCH_ESP32)
  if (LittleFS.begin(false)) return true;
  // Even an old commissioning build flag must not turn mount failure into
  // erase authority. Initialization is an explicit USB maintenance transaction.
  return false;
#else
  LittleFS.setConfig(LittleFSConfig().setAutoFormat(false));
  return LittleFS.begin();
#endif
}
inline int randomBytes(void *out, size_t size) {
#if defined(ARDUINO_ARCH_ESP32)
  esp_fill_random(out, size); return 0;
#else
  return os_get_random(static_cast<unsigned char *>(out), size);
#endif
}
inline uint32_t chipId() {
#if defined(ARDUINO_ARCH_ESP32)
  return static_cast<uint32_t>(ESP.getEfuseMac() >> 24) & 0xffffffU;
#else
  return ESP.getChipId();
#endif
}
inline String resetReason() {
#if defined(ARDUINO_ARCH_ESP32)
  return String(static_cast<unsigned>(esp_reset_reason()));
#else
  return ESP.getResetReason();
#endif
}
inline const char *architecture() {
#if defined(ARDUINO_ARCH_ESP32)
  return "esp32s3";
#else
  return "esp8266";
#endif
}
} // namespace SystemPlatform
