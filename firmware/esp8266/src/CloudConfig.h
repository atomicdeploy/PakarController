#pragma once

#include <Arduino.h>
#include <FS.h>

class CloudConfigStore {
 public:
  bool begin(fs::FS &filesystem);
  bool save();
  bool setMqtt(bool enabled, bool tls, const String &host, uint16_t port,
               const String &user, const String &password,
               const String &baseTopic, bool replacePassword);
  bool setWebSocket(bool enabled, bool tls, const String &host, uint16_t port,
                    const String &path, const String &token,
                    bool replaceToken);
  bool setTelemetrySeconds(uint16_t seconds);

  bool mqttEnabled() const { return record_.mqttEnabled != 0; }
  bool mqttTls() const { return record_.mqttTls != 0; }
  uint16_t mqttPort() const { return record_.mqttPort; }
  const char *mqttHost() const { return record_.mqttHost; }
  const char *mqttUser() const { return record_.mqttUser; }
  const char *mqttPassword() const { return record_.mqttPassword; }
  bool mqttPasswordSet() const { return record_.mqttPassword[0] != '\0'; }
  const char *mqttBaseTopic() const { return record_.mqttBaseTopic; }

  bool webSocketEnabled() const { return record_.webSocketEnabled != 0; }
  bool webSocketTls() const { return record_.webSocketTls != 0; }
  uint16_t webSocketPort() const { return record_.webSocketPort; }
  const char *webSocketHost() const { return record_.webSocketHost; }
  const char *webSocketPath() const { return record_.webSocketPath; }
  const char *webSocketToken() const { return record_.webSocketToken; }
  bool webSocketTokenSet() const { return record_.webSocketToken[0] != '\0'; }
  uint16_t telemetrySeconds() const { return record_.telemetrySeconds; }
  const char *lastError() const { return error_.c_str(); }

 private:
  struct Record {
    uint32_t magic;
    uint8_t schema;
    uint8_t mqttEnabled;
    uint8_t mqttTls;
    uint16_t mqttPort;
    char mqttHost[64];
    char mqttUser[33];
    char mqttPassword[65];
    char mqttBaseTopic[65];
    uint8_t webSocketEnabled;
    uint8_t webSocketTls;
    uint16_t webSocketPort;
    char webSocketHost[64];
    char webSocketPath[97];
    char webSocketToken[65];
    uint16_t telemetrySeconds;
    uint32_t checksum;
  } __attribute__((packed));

  static uint32_t checksum(const Record &record);
  static bool copyChecked(char *target, size_t capacity, const String &value,
                          const __FlashStringHelper *name, String &error);
  void defaults();

  fs::FS *filesystem_ = nullptr;
  Record record_{};
  String error_;
};

extern CloudConfigStore cloudConfig;
