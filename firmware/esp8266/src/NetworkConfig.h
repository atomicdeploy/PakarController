#pragma once

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <FS.h>

// PU850-derived AP+station policy with PakarController-owned, checksum-backed
// LittleFS storage. AP, station and simultaneous mode never block boot while
// infrastructure is absent. At least one interface must remain enabled.
class NetworkConfigStore {
 public:
  bool begin(fs::FS &filesystem);
  bool configure(const char *hostname, const char *apPassword);
  bool setStation(const String &ssid, const String &password, bool enabled, bool replacePassword = true);
  bool setAccessPointEnabled(bool enabled);
  bool setAddressing(bool dhcp, const String &address = String(),
                     const String &gateway = String(), const String &subnet = String(),
                     const String &dns1 = String(), const String &dns2 = String());
  bool save();

  bool accessPointEnabled() const { return record_.accessPointEnabled != 0; }
  bool stationEnabled() const { return record_.stationEnabled != 0; }
  bool stationConnected() const { return WiFi.status() == WL_CONNECTED; }
  const char *stationSsid() const { return record_.stationSsid; }
  bool dhcpEnabled() const { return record_.dhcpEnabled != 0; }
  IPAddress stationAddress() const { return IPAddress(record_.addresses[0]); }
  IPAddress stationGateway() const { return IPAddress(record_.addresses[1]); }
  IPAddress stationSubnet() const { return IPAddress(record_.addresses[2]); }
  IPAddress stationDns1() const { return IPAddress(record_.addresses[3]); }
  IPAddress stationDns2() const { return IPAddress(record_.addresses[4]); }
  const char *lastError() const { return error_.c_str(); }

 private:
  struct Record {
    uint32_t magic;
    uint8_t schema;
    uint8_t stationEnabled;
    uint8_t accessPointEnabled;
    char stationSsid[33];
    char stationPassword[64];
    uint8_t dhcpEnabled;
    uint8_t addresses[5][4];
    uint32_t checksum;
  } __attribute__((packed));

  static uint32_t checksum(const Record &record);
  void defaults();

  fs::FS *filesystem_ = nullptr;
  Record record_{};
  String error_;
};

extern NetworkConfigStore networkConfig;
