#pragma once
#include "FS.h"

// SystemPlatform's new SDK boundary is included by the real WebAccess owner.
// These compile doubles expose ESP8266's LittleFS configuration / ESP identity
// API, not a claim that the middleware test mounts flash or reads a real chip.
class LittleFSConfig {
 public:
  LittleFSConfig &setAutoFormat(bool enabled) { autoFormat = enabled; return *this; }
  bool autoFormat = true;
};
class AccessFixtureLittleFS : public fs::FS {
 public:
  bool setConfig(const LittleFSConfig &value) { config = value; return true; }
  bool begin() { return true; }
  LittleFSConfig config;
};
inline AccessFixtureLittleFS LittleFS;
struct AccessFixtureEsp {
  uint32_t getChipId() const { return 0x123456; }
  String getResetReason() const { return "fixture reset"; }
};
inline AccessFixtureEsp ESP;
