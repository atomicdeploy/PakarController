#include "NetworkConfig.h"
#include "ConfigurationChanges.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr char kPath[] = "/network.bin";
constexpr char kPending[] = "/network.pending";
constexpr uint32_t kMagic = 0x504B4E54UL;  // PKNT
constexpr uint8_t kSchema = 3;

struct LegacyRecord {
  uint32_t magic;
  uint8_t schema;
  uint8_t stationEnabled;
  char stationSsid[33];
  char stationPassword[64];
  uint32_t checksum;
} __attribute__((packed));

struct PreviousRecord {
  uint32_t magic;
  uint8_t schema;
  uint8_t stationEnabled;
  uint8_t accessPointEnabled;
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
  record_.dhcpEnabled = 1;
  record_.checksum = checksum(record_);
}

bool NetworkConfigStore::begin(fs::FS &filesystem) {
  filesystem_ = &filesystem;
  defaults();
  File file = filesystem.open(kPath, "r");
  if (!file) return true;
  if (file.size() == sizeof(PreviousRecord)) {
    PreviousRecord previous{};
    const bool complete = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous)) == sizeof(previous);
    file.close();
    if (!complete || previous.magic != kMagic || previous.schema != 2 ||
        previous.stationEnabled > 1 || previous.accessPointEnabled > 1 ||
        (!previous.stationEnabled && !previous.accessPointEnabled) ||
        (previous.stationEnabled && !previous.stationSsid[0]) ||
        previous.stationSsid[32] != '\0' || previous.stationPassword[63] != '\0' ||
        previous.checksum != checksumBytes(reinterpret_cast<const uint8_t *>(&previous), offsetof(PreviousRecord, checksum))) {
      error_ = F("network settings checksum is invalid; AP-only defaults loaded"); return false;
    }
    record_.stationEnabled = previous.stationEnabled;
    record_.accessPointEnabled = previous.accessPointEnabled;
    memcpy(record_.stationSsid, previous.stationSsid, sizeof(record_.stationSsid));
    memcpy(record_.stationPassword, previous.stationPassword, sizeof(record_.stationPassword));
    return save(); // Retain credentials/mode, default newly added addressing to DHCP.
  }
  if (file.size() == sizeof(LegacyRecord)) {
    LegacyRecord legacy{};
    const bool complete = file.read(reinterpret_cast<uint8_t *>(&legacy),
                                    sizeof(legacy)) == sizeof(legacy);
    file.close();
    const bool valid =
        complete && legacy.magic == kMagic && legacy.schema == 1 && legacy.stationEnabled <= 1 &&
        (!legacy.stationEnabled || legacy.stationSsid[0]) &&
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
  const bool exact = file.size() == sizeof(stored);
  const bool complete = exact && file.read(reinterpret_cast<uint8_t *>(&stored),
                                  sizeof(stored)) == sizeof(stored);
  file.close();
  if (!complete || stored.magic != kMagic || stored.schema != kSchema ||
      stored.stationEnabled > 1 || stored.accessPointEnabled > 1 || stored.dhcpEnabled > 1 ||
      (!stored.stationEnabled && !stored.accessPointEnabled && !ethernetBuild()) ||
      (stored.stationEnabled && !stored.stationSsid[0]) ||
      stored.stationSsid[sizeof(stored.stationSsid) - 1] != '\0' ||
      stored.stationPassword[sizeof(stored.stationPassword) - 1] != '\0' ||
      stored.checksum != checksum(stored)) {
    error_ = F("network settings checksum is invalid; AP-only defaults loaded");
    return false;
  }
  record_ = stored;
  if (!dhcpEnabled() && !setAddressing(false, stationAddress().toString(),
      stationGateway().toString(), stationSubnet().toString(), stationDns1().toString(), stationDns2().toString())) {
    defaults(); error_ = F("network static addressing is invalid; AP-only defaults loaded"); return false;
  }
  return true;
}

bool NetworkConfigStore::setStation(const String &ssid, const String &password,
                                    bool enabled, bool replacePassword) {
  if (ssid.length() != strlen(ssid.c_str()) || password.length() != strlen(password.c_str()) ||
      ssid.length() > 32 || password.length() > 63 || (enabled && ssid.length() == 0)) {
    error_ = F("station SSID must be 1..32 and password 0..63 bytes");
    return false;
  }
  if (!replacePassword && ssid != record_.stationSsid) {
    error_ = F("changing station SSID requires an explicit password or Open choice"); return false;
  }
  memset(record_.stationSsid, 0, sizeof(record_.stationSsid));
  ssid.toCharArray(record_.stationSsid, sizeof(record_.stationSsid));
  if (replacePassword) {
    memset(record_.stationPassword, 0, sizeof(record_.stationPassword));
    password.toCharArray(record_.stationPassword, sizeof(record_.stationPassword));
  }
  record_.stationEnabled = enabled ? 1 : 0;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}

bool NetworkConfigStore::setAccessPointEnabled(bool enabled) {
  if (!enabled && !stationEnabled() && !ethernetBuild()) {
    error_ = F("AP and station cannot both be disabled");
    return false;
  }
  record_.accessPointEnabled = enabled ? 1 : 0;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}

bool NetworkConfigStore::save() {
  if (!stationEnabled() && !accessPointEnabled() && !ethernetBuild()) {
    error_ = F("AP and station cannot both be disabled"); return false;
  }
  if (filesystem_ == nullptr) {
    error_ = F("LittleFS is not mounted");
    return false;
  }
  record_.checksum = checksum(record_);
  // Skip identical writes. Stage, verify and rename instead of truncating the
  // only credential copy; a failed candidate never replaces the active record.
  File current = filesystem_->open(kPath, "r"); Record previous{};
  const bool identical = current && current.size() == sizeof(record_) &&
      current.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous)) == sizeof(previous) &&
      memcmp(&previous, &record_, sizeof(previous)) == 0;
  if (current) current.close();
  if (identical) { error_ = String(); return true; }
  File file = filesystem_->open(kPending, "w");
  if (!file) {
    error_ = F("cannot create network settings");
    return false;
  }
  const bool complete = file.write(reinterpret_cast<const uint8_t *>(&record_),
                                   sizeof(record_)) == sizeof(record_);
  file.close();
  File verify = filesystem_->open(kPending, "r"); Record readback{};
  const bool verified = verify && verify.size() == sizeof(record_) &&
      verify.read(reinterpret_cast<uint8_t *>(&readback), sizeof(readback)) == sizeof(readback) &&
      memcmp(&readback, &record_, sizeof(readback)) == 0;
  if (verify) verify.close();
  if (!complete || !verified || !filesystem_->rename(kPending, kPath)) {
    error_ = F("network settings commit failed; previous settings preserved"); return false;
  }
  error_ = String(); PakarConfiguration::changes().saved(PakarConfiguration::Network); return true;
}

bool NetworkConfigStore::setAddressing(bool dhcp, const String &address,
    const String &gateway, const String &subnet, const String &dns1, const String &dns2) {
  uint8_t parsed[5][4]{};
  if (!dhcp) {
    const String values[] = {address, gateway, subnet, dns1, dns2};
    uint32_t numbers[5]{};
    for (unsigned index = 0; index < 5; ++index) {
      IPAddress ip;
      if ((index >= 3 && !values[index].length())) continue;
      if (values[index].length() != strlen(values[index].c_str()) || !ip.fromString(values[index])) {
        error_ = F("static address, gateway, subnet and DNS must be IPv4 values"); return false;
      }
      for (unsigned byte = 0; byte < 4; ++byte) {
        parsed[index][byte] = ip[byte]; numbers[index] = (numbers[index] << 8) | ip[byte];
      }
    }
    const uint32_t hostMask = ~numbers[2];
    if (hostMask < 3 || hostMask == UINT32_MAX || (hostMask & (hostMask + 1)) ||
        (numbers[0] >> 24) == 0 || (numbers[0] >> 24) >= 224 ||
        (numbers[0] & hostMask) == 0 || (numbers[0] & hostMask) == hostMask ||
        (numbers[1] >> 24) == 0 || (numbers[1] >> 24) >= 224 ||
        (numbers[0] & numbers[2]) != (numbers[1] & numbers[2]) ||
        (numbers[1] & hostMask) == 0 || (numbers[1] & hostMask) == hostMask ||
        numbers[0] == numbers[1] || (numbers[3] >> 24) >= 224 || (numbers[4] >> 24) >= 224) {
      error_ = F("invalid static subnet, host, gateway or DNS address"); return false;
    }
  }
  record_.dhcpEnabled = dhcp ? 1 : 0;
  // Preserve the last validated static profile while DHCP is active so the
  // front panel can switch back without re-entering every address.
  if (!dhcp) memcpy(record_.addresses, parsed, sizeof(parsed));
  record_.checksum = checksum(record_); error_ = String(); return true;
}

bool NetworkConfigStore::configure(const char *hostname,
                                   const char *apPassword) {
  if (!hostname || !*hostname || strlen(hostname)>63) {
    error_ = F("network hostname must be 1..63 bytes");return false;
  }
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
#if !defined(ESP32)
  WiFi.setAutoConnect(false);
#endif
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  delay(10);
  const bool apEnabled = accessPointEnabled();
  if (!apEnabled && !stationEnabled()) {
    // Do not accidentally select WIFI_STA for a persisted Ethernet-only mode.
    // A build without Ethernet cannot load/save such a record in the first place.
    error_ = String();
    return false; // AP is genuinely not running, not a network failure.
  }
#if defined(ESP32)
  // ASA0002E WiFi_Setup applies its persisted identity BEFORE enabling Wi-Fi.
  // Arduino-ESP32 mode() copies Network's default into the new STA netif.
  if (!WiFi.setHostname(hostname)) { error_ = F("Cannot set network hostname"); return false; }
  // Set the SDK's retained policy before STA starts, and reapply below once
  // started: mode changes/reconnects must not restore implicit modem sleep.
  WiFi.setSleep(PAKAR_S3_WIFI_MODEM_SLEEP != 0);
#endif
  if (!WiFi.mode(apEnabled ? (stationEnabled() ? WIFI_AP_STA : WIFI_AP) : WIFI_STA)) {
    error_ = F("Cannot start requested Wi-Fi mode"); return false;
  }
#if defined(ESP32)
  const bool sleepPolicyApplied = WiFi.setSleep(PAKAR_S3_WIFI_MODEM_SLEEP != 0);
  // Network's global buffer is only 32 bytes in SDK 3.3.12. Apply the full
  // validated DNS label to each netif before addressing/association as well.
  if ((stationEnabled() && (!WiFi.STA.setHostname(hostname) ||
          !WiFi.STA.getHostname() || strcmp(WiFi.STA.getHostname(), hostname) != 0)) ||
      (apEnabled && (!WiFi.softAPsetHostname(hostname) ||
          !WiFi.softAPgetHostname() || strcmp(WiFi.softAPgetHostname(), hostname) != 0))) {
    WiFi.mode(WIFI_OFF);
    error_ = F("Wi-Fi hostname was not applied; interfaces stopped"); return false;
  }
#else
  if (!WiFi.hostname(hostname)) { error_ = F("Cannot set station hostname"); return false; }
#endif
  bool ap = false;
  // DNS host labels permit63bytes, IEEE802.11 SSIDs only32. The SDK copies32
  // but uses strlen(input) for ssid_len; pass an explicitly bounded AP name.
  // The full persisted hostname/netif/mDNS identity is never silently renamed.
  const String apSsid=String(hostname).substring(0,32);
  if (apEnabled) ap = WiFi.softAP(apSsid.c_str(), apPassword);
  if (stationEnabled()) {
    const bool addressed = dhcpEnabled()
        ? WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0))
        : WiFi.config(stationAddress(), stationGateway(), stationSubnet(), stationDns1(), stationDns2());
    if (!addressed) { error_ = F("station IP configuration failed"); return ap; }
    WiFi.begin(record_.stationSsid, record_.stationPassword);
    WiFi.setAutoReconnect(true);
#if !defined(ESP32)
    WiFi.setAutoConnect(true);
#endif
  }
  if (apEnabled && !ap) error_ = F("access point startup failed");
#if defined(ESP32)
  else if (!sleepPolicyApplied) error_ = F("Wi-Fi latency policy was not applied");
#endif
  else error_ = String();
  // Return AP availability for existing callers; STA-only is not an AP startup.
  return ap;
}
