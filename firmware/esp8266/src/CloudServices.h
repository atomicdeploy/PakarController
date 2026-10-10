#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WebSocketsClient.h>
#include <WiFiClientSecureBearSSL.h>

#include "CloudConfig.h"

class CloudServices {
 public:
  using CommandHandler = bool (*)(const String &message, String &result);
  using StatusProvider = String (*)();

  void begin(const String &deviceId, CommandHandler handler,
             StatusProvider statusProvider);
  void applyConfiguration();
  void renameDevice(const String &deviceId);
  void service(bool networkReady, bool timeReady, uint32_t now);

  bool mqttConnected() { return mqtt_.connected(); }
  int mqttState() { return mqtt_.state(); }
  bool webSocketConnected() { return webSocket_.isConnected(); }
  uint32_t mqttMessagesReceived() const { return mqttMessagesReceived_; }
  uint32_t webSocketMessagesReceived() const {
    return webSocketMessagesReceived_;
  }
  const String &lastError() const { return lastError_; }

 private:
  static void mqttCallback(char *topic, uint8_t *payload, unsigned int length);
  static void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
  void handleCommand(const uint8_t *payload, size_t length, bool fromMqtt);
  void configureWebSocket();
  void connectMqtt(uint32_t now);
  void publishTelemetry(const String &statusJson, uint32_t now);
  String topic(const __FlashStringHelper *suffix) const;

  WiFiClient plainClient_;
  BearSSL::WiFiClientSecure secureClient_;
  PubSubClient mqtt_;
  WebSocketsClient webSocket_;
  String deviceId_;
  String authorizationHeader_;
  String lastError_;
  CommandHandler commandHandler_ = nullptr;
  StatusProvider statusProvider_ = nullptr;
  uint32_t nextMqttAttemptAt_ = 0;
  uint32_t lastTelemetryAt_ = 0;
  uint32_t mqttMessagesReceived_ = 0;
  uint32_t webSocketMessagesReceived_ = 0;
  bool webSocketConfigured_ = false;
};

extern CloudServices cloudServices;
