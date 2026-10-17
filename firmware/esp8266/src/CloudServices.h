#pragma once

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#else
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#endif
#include <PubSubClient.h>
#include <WebSocketsClient.h>

#include "CloudConfig.h"

class CloudServices {
 public:
  using CommandHandler = bool (*)(const String &message, String &result);
  using StatusProvider = String (*)();

  void begin(const String &deviceId, CommandHandler handler,
             StatusProvider statusProvider,StatusProvider telemetryProvider=nullptr);
  void suspend();
  void requestTelemetry() {lastTelemetryAt_=millis()-static_cast<uint32_t>(cloudConfig.telemetrySeconds())*1000U;}
  void applyConfiguration();
  void renameDevice(const String &deviceId);
  void service(bool networkReady, bool timeReady, uint32_t now);
  void publishEvent(const String &event);

  bool mqttConnected() { return networkReady_ && mqtt_.connected(); }
  int mqttState() { return mqtt_.state(); }
  bool webSocketConnected() { return networkReady_ && webSocket_.isConnected(); }
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
#if defined(ESP32)
  WiFiClientSecure secureClient_;
#else
  BearSSL::WiFiClientSecure secureClient_;
#endif
  PubSubClient mqtt_;
  WebSocketsClient webSocket_;
  String deviceId_;
  String telemetryBoot_;
  String authorizationHeader_;
  String lastError_;
  CommandHandler commandHandler_ = nullptr;
  StatusProvider statusProvider_ = nullptr;
  StatusProvider telemetryProvider_ = nullptr;
  uint32_t snapshotSequence_=0;
  uint32_t nextMqttAttemptAt_ = 0;
  uint32_t lastTelemetryAt_ = 0;
  uint32_t mqttMessagesReceived_ = 0;
  uint32_t webSocketMessagesReceived_ = 0;
  bool webSocketConfigured_ = false;
  bool networkReady_ = false;
};

extern CloudServices cloudServices;
// Shared pinned trust roots; neither firmware fetch nor cloud uses setInsecure().
const char *pakarTrustAnchorPem();
#if !defined(ESP32)
BearSSL::X509List &pakarTrustAnchor();
#endif
