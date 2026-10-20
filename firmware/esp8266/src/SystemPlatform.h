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
#if defined(PAKAR_INITIALIZE_FILESYSTEM) && PAKAR_INITIALIZE_FILESYSTEM
  // Explicit first-USB-commissioning build ONLY, after original flash backup.
  // Record the attempt durably BEFORE format so corruption/power-loss never
  // causes a loop of destructive formatting on subsequent boots.
  nvs_handle_t handle;
  if (nvs_open("pakar-setup", NVS_READWRITE, &handle) != ESP_OK) return false;
  uint8_t attempted = 0;
  const esp_err_t found = nvs_get_u8(handle, "fs-init", &attempted);
  bool allowed = found == ESP_ERR_NVS_NOT_FOUND;
  if (allowed) allowed = nvs_set_u8(handle, "fs-init", 1) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return allowed && LittleFS.format() && LittleFS.begin(false);
#else
  return false; // Physical recovery, never silently reset damaged credentials.
#endif
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
