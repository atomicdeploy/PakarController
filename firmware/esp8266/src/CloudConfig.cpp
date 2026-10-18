#include "CloudConfig.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr char kPath[] = "/cloud.bin";
constexpr char kPending[] = "/cloud.pending";
constexpr uint32_t kMagic = 0x504B434CUL;  // PKCL
constexpr uint8_t kSchema = 1;

uint32_t checksumBytes(const uint8_t *bytes, size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < length; ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

// All stored strings are canonical zero-padded C strings, not a visible
// prefix followed by hidden bytes. This also bounds every later strlen.
bool canonicalString(const char *value, size_t capacity) {
  bool ended = false;
  for (size_t index = 0; index < capacity; ++index) {
    if (value[index] == '\0') ended = true;
    else if (ended) return false;
  }
  return ended;
}

bool validHost(const char *value) {
  for (size_t index = 0; value[index]; ++index) {
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':')) return false;
  }
  return true; // Empty is allowed only for a disabled service below.
}

bool validText(const char *value, bool path = false, bool topic = false) {
  for (size_t index = 0; value[index]; ++index) {
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if (c < (path ? 0x21 : 0x20) || c == 0x7f ||
        (topic && (c == '+' || c == '#'))) return false;
  }
  return true; // UTF-8 data is retained rather than treated as ASCII controls.
}
}  // namespace

CloudConfigStore cloudConfig;

uint32_t CloudConfigStore::checksum(const Record &record) {
  return checksumBytes(reinterpret_cast<const uint8_t *>(&record),
                       offsetof(Record, checksum));
}

bool CloudConfigStore::validRecord(const Record &r) {
  if (r.magic != kMagic || r.schema != kSchema || r.mqttEnabled > 1 ||
      r.mqttTls > 1 || r.webSocketEnabled > 1 || r.webSocketTls > 1 ||
      r.mqttPort == 0 || r.webSocketPort == 0 || r.telemetrySeconds < 1 ||
      r.telemetrySeconds > 3600 || r.checksum != checksum(r)) return false;
  if (!canonicalString(r.mqttHost, sizeof(r.mqttHost)) ||
      !canonicalString(r.mqttUser, sizeof(r.mqttUser)) ||
      !canonicalString(r.mqttPassword, sizeof(r.mqttPassword)) ||
      !canonicalString(r.mqttBaseTopic, sizeof(r.mqttBaseTopic)) ||
      !canonicalString(r.webSocketHost, sizeof(r.webSocketHost)) ||
      !canonicalString(r.webSocketPath, sizeof(r.webSocketPath)) ||
      !canonicalString(r.webSocketToken, sizeof(r.webSocketToken))) return false;
  return (!r.mqttEnabled || r.mqttHost[0]) && validHost(r.mqttHost) &&
      r.mqttBaseTopic[0] && validText(r.mqttBaseTopic, false, true) &&
      (!r.webSocketEnabled || r.webSocketHost[0]) && validHost(r.webSocketHost) &&
      r.webSocketPath[0] == '/' && validText(r.webSocketPath, true) &&
      validText(r.webSocketToken);
}

bool CloudConfigStore::copyChecked(char *target, size_t capacity,
                                   const String &value,
                                   const __FlashStringHelper *name,
                                   String &error) {
  if (value.length() >= capacity || value.length() != strlen(value.c_str())) {
    error = String(name) + F(" is too long or contains NUL");
    return false;
  }
  memset(target, 0, capacity);
  value.toCharArray(target, capacity);
  return true;
}

void CloudConfigStore::defaults() {
  memset(&record_, 0, sizeof(record_));
  record_.magic = kMagic;
  record_.schema = kSchema;
  record_.mqttTls = 1;
  record_.mqttPort = 8883;
  strlcpy(record_.mqttHost, "mqtt.digitalogic.ir", sizeof(record_.mqttHost));
  strlcpy(record_.mqttBaseTopic, "pakarcontroller", sizeof(record_.mqttBaseTopic));
  record_.webSocketPort = 443;
  strlcpy(record_.webSocketPath, "/", sizeof(record_.webSocketPath));
  record_.telemetrySeconds = 5;
  record_.checksum = checksum(record_);
}

bool CloudConfigStore::begin(fs::FS &filesystem) {
  filesystem_ = &filesystem;
  defaults();
  error_ = String();
  File file = filesystem.open(kPath, "r");
  if (!file) return true;
  Record stored{};
  const bool complete =
      file.size() == sizeof(stored) &&
      file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) ==
          sizeof(stored);
  file.close();
  if (!complete || !validRecord(stored)) {
    error_ = F("cloud settings checksum is invalid; safe disabled defaults loaded");
    return false;
  }
  record_ = stored;
  return true;
}

bool CloudConfigStore::save() {
  if (filesystem_ == nullptr) {
    error_ = F("LittleFS is not mounted");
    return false;
  }
  record_.checksum = checksum(record_);
  if (!validRecord(record_)) {
    error_ = F("invalid cloud settings; previous settings preserved"); return false;
  }
  // Adapt the actual NetworkConfigStore pending/readback/rename transaction.
  // Never truncate the sole credential copy; identical saves touch no flash.
  File current = filesystem_->open(kPath, "r"); Record previous{};
  const bool identical = current && current.size() == sizeof(record_) &&
      current.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous)) == sizeof(previous) &&
      memcmp(&previous, &record_, sizeof(previous)) == 0;
  if (current) current.close();
  if (identical) { error_ = String(); return true; }
  File file = filesystem_->open(kPending, "w");
  if (!file) {
    error_ = F("cannot create cloud settings");
    return false;
  }
  const bool complete =
      file.write(reinterpret_cast<const uint8_t *>(&record_), sizeof(record_)) ==
      sizeof(record_);
  file.close();
  File verify = filesystem_->open(kPending, "r"); Record readback{};
  const bool verified = verify && verify.size() == sizeof(record_) &&
      verify.read(reinterpret_cast<uint8_t *>(&readback), sizeof(readback)) == sizeof(readback) &&
      memcmp(&readback, &record_, sizeof(readback)) == 0;
  if (verify) verify.close();
  if (!complete || !verified || !filesystem_->rename(kPending, kPath)) {
    error_ = F("cloud settings commit failed; previous settings preserved"); return false;
  }
  error_ = String(); return true;
}

bool CloudConfigStore::setMqtt(bool enabled, bool tls, const String &host,
                               uint16_t port, const String &user,
                               const String &password,
                               const String &baseTopic,
                               bool replacePassword) {
  if (password.length() >= sizeof(record_.mqttPassword) ||
      password.length() != strlen(password.c_str())) {
    error_ = F("MQTT password is too long or contains NUL"); return false;
  }
  if (port == 0 || baseTopic.length() == 0 ||
      (enabled && host.length() == 0)) {
    error_ = F("MQTT host, port, and base topic are required");
    return false;
  }
  Record candidate = record_;
  if (!copyChecked(candidate.mqttHost, sizeof(candidate.mqttHost), host,
                   F("MQTT host"), error_) ||
      !copyChecked(candidate.mqttUser, sizeof(candidate.mqttUser), user,
                   F("MQTT user"), error_) ||
      !copyChecked(candidate.mqttBaseTopic, sizeof(candidate.mqttBaseTopic),
                   baseTopic, F("MQTT base topic"), error_) ||
      (replacePassword &&
       !copyChecked(candidate.mqttPassword, sizeof(candidate.mqttPassword),
                    password, F("MQTT password"), error_))) {
    return false;
  }
  candidate.mqttEnabled = enabled ? 1 : 0;
  candidate.mqttTls = tls ? 1 : 0;
  candidate.mqttPort = port;
  candidate.checksum = checksum(candidate);
  if (!validRecord(candidate)) {
    error_ = F("invalid MQTT host or base topic"); return false;
  }
  record_ = candidate;
  error_ = String();
  return true;
}

bool CloudConfigStore::setWebSocket(bool enabled, bool tls,
                                    const String &host, uint16_t port,
                                    const String &path, const String &token,
                                    bool replaceToken) {
  if (token.length() >= sizeof(record_.webSocketToken) ||
      token.length() != strlen(token.c_str()) || !validText(token.c_str())) {
    error_ = F("WebSocket token is too long or contains control bytes"); return false;
  }
  if (port == 0 || path.length() == 0 || path[0] != '/' ||
      (enabled && host.length() == 0)) {
    error_ = F("WebSocket host, port, and slash-prefixed path are required");
    return false;
  }
  Record candidate = record_;
  if (!copyChecked(candidate.webSocketHost, sizeof(candidate.webSocketHost),
                   host, F("WebSocket host"), error_) ||
      !copyChecked(candidate.webSocketPath, sizeof(candidate.webSocketPath),
                   path, F("WebSocket path"), error_) ||
      (replaceToken &&
       !copyChecked(candidate.webSocketToken,
                    sizeof(candidate.webSocketToken), token,
                    F("WebSocket token"), error_))) {
    return false;
  }
  candidate.webSocketEnabled = enabled ? 1 : 0;
  candidate.webSocketTls = tls ? 1 : 0;
  candidate.webSocketPort = port;
  candidate.checksum = checksum(candidate);
  if (!validRecord(candidate)) {
    error_ = F("invalid WebSocket host, path or token"); return false;
  }
  record_ = candidate;
  error_ = String();
  return true;
}

bool CloudConfigStore::setTelemetrySeconds(uint16_t seconds) {
  if (seconds < 1 || seconds > 3600) {
    error_ = F("telemetry interval must be 1..3600 seconds");
    return false;
  }
  record_.telemetrySeconds = seconds;
  record_.checksum = checksum(record_);
  error_ = String();
  return true;
}
