#include "CloudConfig.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr char kPath[] = "/cloud.bin";
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
}  // namespace

CloudConfigStore cloudConfig;

uint32_t CloudConfigStore::checksum(const Record &record) {
  return checksumBytes(reinterpret_cast<const uint8_t *>(&record),
                       offsetof(Record, checksum));
}

bool CloudConfigStore::copyChecked(char *target, size_t capacity,
                                   const String &value,
                                   const __FlashStringHelper *name,
                                   String &error) {
  if (value.length() >= capacity) {
    error = String(name) + F(" is too long");
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
  File file = filesystem.open(kPath, "r");
  if (!file) return true;
  Record stored{};
  const bool complete =
      file.size() == sizeof(stored) &&
      file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) ==
          sizeof(stored);
  file.close();
  const bool terminated =
      stored.mqttHost[sizeof(stored.mqttHost) - 1] == '\0' &&
      stored.mqttUser[sizeof(stored.mqttUser) - 1] == '\0' &&
      stored.mqttPassword[sizeof(stored.mqttPassword) - 1] == '\0' &&
      stored.mqttBaseTopic[sizeof(stored.mqttBaseTopic) - 1] == '\0' &&
      stored.webSocketHost[sizeof(stored.webSocketHost) - 1] == '\0' &&
      stored.webSocketPath[sizeof(stored.webSocketPath) - 1] == '\0' &&
      stored.webSocketToken[sizeof(stored.webSocketToken) - 1] == '\0';
  if (!complete || !terminated || stored.magic != kMagic ||
      stored.schema != kSchema || stored.mqttPort == 0 ||
      stored.webSocketPort == 0 || stored.telemetrySeconds < 1 ||
      stored.telemetrySeconds > 3600 || stored.checksum != checksum(stored)) {
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
  File file = filesystem_->open(kPath, "w");
  if (!file) {
    error_ = F("cannot create cloud settings");
    return false;
  }
  const bool complete =
      file.write(reinterpret_cast<const uint8_t *>(&record_), sizeof(record_)) ==
      sizeof(record_);
  file.close();
  if (!complete) error_ = F("cloud settings write failed");
  return complete;
}

bool CloudConfigStore::setMqtt(bool enabled, bool tls, const String &host,
                               uint16_t port, const String &user,
                               const String &password,
                               const String &baseTopic,
                               bool replacePassword) {
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
  record_ = candidate;
  error_ = String();
  return true;
}

bool CloudConfigStore::setWebSocket(bool enabled, bool tls,
                                    const String &host, uint16_t port,
                                    const String &path, const String &token,
                                    bool replaceToken) {
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
