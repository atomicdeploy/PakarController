#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>

// PU850-derived AP+station policy with PakarController-owned, checksum-backed
// LittleFS storage. The recovery AP always remains available; station mode is
// optional and never blocks boot while an infrastructure network is absent.
class NetworkConfigStore {
 public:
  bool begin(fs::FS &filesystem);
  bool configure(const char *hostname, const char *apPassword);
  bool setStation(const String &ssid, const String &password, bool enabled);
  bool save();

  bool stationEnabled() const { return record_.stationEnabled != 0; }
  bool stationConnected() const { return WiFi.status() == WL_CONNECTED; }
  const char *stationSsid() const { return record_.stationSsid; }
  const char *lastError() const { return error_.c_str(); }

 private:
  struct Record {
    uint32_t magic;
    uint8_t schema;
    uint8_t stationEnabled;
    char stationSsid[33];
    char stationPassword[64];
    uint32_t checksum;
  } __attribute__((packed));

  static uint32_t checksum(const Record &record);
  void defaults();

  fs::FS *filesystem_ = nullptr;
  Record record_{};
  String error_;
};

extern NetworkConfigStore networkConfig;
