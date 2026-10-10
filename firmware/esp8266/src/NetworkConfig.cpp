#include "NetworkConfig.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr char kPath[] = "/network.bin";
constexpr uint32_t kMagic = 0x504B4E54UL;  // PKNT
constexpr uint8_t kSchema = 2;

struct LegacyRecord {
  uint32_t magic;
  uint8_t schema;
  uint8_t stationEnabled;
  char stationSsid[33];
  char stationPassword[64];
  uint32_t checksum;
} __attribute__((packed));

uint32_t checksumBytes(const uint8_t *bytes, size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < length; ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}
}

NetworkConfigStore networkConfig;

uint32_t NetworkConfigStore::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  return checksumBytes(bytes, offsetof(Record, checksum));
}

void NetworkConfigStore::defaults() {
  memset(&record_, 0, sizeof(record_));
  record_.magic = kMagic;
  record_.schema = kSchema;
  record_.accessPointEnabled = 1;
  record_.checksum = checksum(record_);
}

bool NetworkConfigStore::begin(fs::FS &filesystem) {
  filesystem_ = &filesystem;
  defaults();
  File file = filesystem.open(kPath, "r");
  if (!file) return true;
  if (file.size() == sizeof(LegacyRecord)) {
    LegacyRecord legacy{};
    const bool complete = file.read(reinterpret_cast<uint8_t *>(&legacy),
                                    sizeof(legacy)) == sizeof(legacy);
    file.close();
    const bool valid =
        complete && legacy.magic == kMagic && legacy.schema == 1 &&
        legacy.stationSsid[sizeof(legacy.stationSsid) - 1] == '\0' &&
        legacy.stationPassword[sizeof(legacy.stationPassword) - 1] == '\0' &&
        legacy.checksum == checksumBytes(
                               reinterpret_cast<const uint8_t *>(&legacy),
                               offsetof(LegacyRecord, checksum));
    if (!valid) {
      error_ = F("legacy network settings checksum is invalid; AP-only defaults loaded");
      return false;
    }
    record_.stationEnabled = legacy.stationEnabled;
    memcpy(record_.stationSsid, legacy.stationSsid, sizeof(record_.stationSsid));
    memcpy(record_.stationPassword, legacy.stationPassword,
           sizeof(record_.stationPassword));
    record_.checksum = checksum(record_);
    return save();
  }
  Record stored{};
  const bool complete = file.read(reinterpret_cast<uint8_t *>(&stored),
                                  sizeof(stored)) == sizeof(stored);
  file.close();
  if (!complete || stored.magic != kMagic || stored.schema != kSchema ||
      stored.stationSsid[sizeof(stored.stationSsid) - 1] != '\0' ||
      stored.stationPassword[sizeof(stored.stationPassword) - 1] != '\0' ||
      stored.checksum != checksum(stored)) {
    error_ = F("network settings checksum is invalid; AP-only defaults loaded");
    return false;
  }
  record_ = stored;
  return true;
}

bool NetworkConfigStore::setStation(const String &ssid, const String &password,
                                    bool enabled) {
  if (enabled && (ssid.length() == 0 || ssid.length() > 32 ||
                  password.length() > 63)) {
    error_ = F("station SSID must be 1..32 and password 0..63 bytes");
    return false;
  }
  memset(record_.stationSsid, 0, sizeof(record_.stationSsid));
  memset(record_.stationPassword, 0, sizeof(record_.stationPassword));
  ssid.toCharArray(record_.stationSsid, sizeof(record_.stationSsid));
  password.toCharArray(record_.stationPassword, sizeof(record_.stationPassword));
  record_.stationEnabled = enabled ? 1 : 0;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}

bool NetworkConfigStore::setAccessPointEnabled(bool enabled) {
  if (!enabled && !stationEnabled()) {
    error_ = F("AP and station cannot both be disabled");
    return false;
  }
  record_.accessPointEnabled = enabled ? 1 : 0;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}

bool NetworkConfigStore::save() {
  if (filesystem_ == nullptr) {
    error_ = F("LittleFS is not mounted");
    return false;
  }
  record_.checksum = checksum(record_);
  File file = filesystem_->open(kPath, "w");
  if (!file) {
    error_ = F("cannot create network settings");
    return false;
  }
  const bool complete = file.write(reinterpret_cast<const uint8_t *>(&record_),
                                   sizeof(record_)) == sizeof(record_);
  file.close();
  if (!complete) error_ = F("network settings write failed");
  return complete;
}

bool NetworkConfigStore::configure(const char *hostname,
                                   const char *apPassword) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  delay(10);
  const bool apEnabled = accessPointEnabled();
  WiFi.mode(apEnabled ? (stationEnabled() ? WIFI_AP_STA : WIFI_AP) : WIFI_STA);
  WiFi.hostname(hostname);
  bool ap = false;
  if (apEnabled) ap = WiFi.softAP(hostname, apPassword);
  if (stationEnabled()) {
    WiFi.begin(record_.stationSsid, record_.stationPassword);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
  }
  return ap;
}
