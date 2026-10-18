#pragma once
#include <Arduino.h>
#include <FS.h>
#include <AccessPolicy.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <atomic>
#endif

class AccessConfig {
 public:
  bool begin(fs::FS &filesystem, bool mounted = true);
  bool change(const String &currentPassword, const String &username, const String &password);
  bool configure(const String &currentPassword, bool enabled,
                 const String &username = String(), const String &password = String());
  // Called only by the physically connected USB protocol, never a LAN/cloud route.
  bool provisionFromPhysicalUsb(const String &username, const String &password);
  const char *username() const { return record_.username; }
  const char *password() const { return record_.password; }
  bool ready() const { return ready_; }
  uint32_t securityRevision() const { return securityRevision_; }
  bool enabled() const { return ready_ && record_.enabled == 1; }
  bool defaultsActive() const {
    return ready_ && BoardCore::Access::equal(username(), "admin") &&
           BoardCore::Access::equal(password(), "pakarcontroller");
  }
  bool verify(const char *user, const char *pass) const {
    return ready_ && BoardCore::Access::equal(user, username()) && BoardCore::Access::equal(pass, password());
  }
  const String &error() const { return error_; }
 private:
  struct Record {
    uint32_t magic;
    uint8_t schema;
    char username[33];
    char password[65];
    uint8_t enabled;
    uint32_t checksum;
  } __attribute__((packed)) record_{};
  static uint32_t checksum(const Record &record);
  bool save(const Record &next);
  fs::FS *filesystem_ = nullptr;
  bool ready_ = false;
#if defined(ARDUINO_ARCH_ESP32)
  std::atomic<uint32_t> securityRevision_{1};
#else
  uint32_t securityRevision_ = 1;
#endif
  String error_;
};
extern AccessConfig accessConfig;
