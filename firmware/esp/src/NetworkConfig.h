#pragma once

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <FS.h>

// This externally powered controller prioritizes command/OTA latency. Arduino
// ESP32 otherwise defaults to MIN_MODEM and defers receives to the AP's DTIM.
// Opt-in power saving is a deliberate build profile, not a lost settings file.
#ifndef PAKAR_S3_WIFI_MODEM_SLEEP
#define PAKAR_S3_WIFI_MODEM_SLEEP 0
#endif
#if PAKAR_S3_WIFI_MODEM_SLEEP != 0 && PAKAR_S3_WIFI_MODEM_SLEEP != 1
#error "PAKAR_S3_WIFI_MODEM_SLEEP must be 0 or 1"
#endif

// PU850-derived AP+station policy with PakarController-owned, checksum-backed
// LittleFS storage. AP, station and simultaneous mode never block boot while
// infrastructure is absent. Wi-Fi may be fully off in a native Ethernet build;
// the shared mutation owner separately requires an actually ready Ethernet path.
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
  bool setFallback(uint8_t slot,const String &ssid,const String &password);
  void setAutoSwitch(bool enabled){record_.autoSwitch=enabled;}
  bool autoSwitch()const{return record_.autoSwitch!=0;}
  const char *profileSsid(uint8_t index)const{return index==0?record_.stationSsid:index<=3?record_.fallbackSsid[index-1]:"";}
  uint8_t activeProfile()const{return activeProfile_;}
  void service(uint32_t now);
#if defined(ESP32)
  // Explicit, maintenance-owned AP commissioning. No Record fields or SDK NVS
  // credentials are changed. The owner MUST restore even after a failed join.
  bool beginTemporaryStation(const String &ssid,const String &password,const uint8_t bssid[6]);
  bool temporaryStationReady(const String &ssid,const uint8_t bssid[6])const;
  bool restoreSavedNetwork(const char *hostname,const char *apPassword);
  bool temporaryStationActive()const{return temporaryStation_;}
#endif
  static constexpr bool ethernetBuild() {
#if defined(ESP32) && defined(PAKAR_S3_ETHERNET_ENABLED) && PAKAR_S3_ETHERNET_ENABLED
    return true;
#else
    return false;
#endif
  }

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
    char fallbackSsid[3][33];
    char fallbackPassword[3][64];
    uint8_t autoSwitch;
    uint32_t checksum;
  } __attribute__((packed));

  static uint32_t checksum(const Record &record);
  void defaults();

  fs::FS *filesystem_ = nullptr;
  Record record_{};
  String error_;
  uint8_t activeProfile_=0;
  uint32_t attemptAt_=0,scanAt_=0;
  bool wasConnected_=false,switchScan_=false;
  bool temporaryStation_=false;
};

extern NetworkConfigStore networkConfig;
