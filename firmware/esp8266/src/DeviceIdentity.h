#pragma once
#include <Arduino.h>
#include <FS.h>
#include <DeviceName.h>
#ifndef PAKAR_DEFAULT_HOSTNAME
#ifdef PAKAR_INITIAL_NAME
#define PAKAR_NAME_STRINGIFY_(value) #value
#define PAKAR_NAME_STRINGIFY(value) PAKAR_NAME_STRINGIFY_(value)
#define PAKAR_DEFAULT_HOSTNAME PAKAR_NAME_STRINGIFY(PAKAR_INITIAL_NAME)
#else
#define PAKAR_DEFAULT_HOSTNAME "pakarcontroller"
#endif
#endif
class DeviceIdentity {
 public:
  bool begin(fs::FS &filesystem, bool mounted = true);
  bool rename(const String &name);
  bool configure(const String &hostname, const String &displayName);
  const char *hostname() const { return record_.hostname; }
  const char *displayName() const { return record_.displayName; }
  bool ready() const { return ready_; }
  const String &error() const { return error_; }
 private:
  struct Record {
    uint32_t magic;
    uint8_t schema;
    char hostname[64];
    char displayName[64];
    uint32_t checksum;
  } __attribute__((packed)) record_{};
  static uint32_t checksum(const Record &record);
  fs::FS *filesystem_ = nullptr;
  bool ready_ = false;
  String error_;
};
extern DeviceIdentity deviceIdentity;
