#include "CloudServices.h"

namespace {
const char kIsrgRootX1[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)CERT";

BearSSL::X509List trustAnchor(kIsrgRootX1);
}  // namespace

CloudServices cloudServices;

void CloudServices::begin(const String &deviceId, CommandHandler handler,
                          StatusProvider statusProvider) {
  deviceId_ = deviceId;
  commandHandler_ = handler;
  statusProvider_ = statusProvider;
  secureClient_.setTrustAnchors(&trustAnchor);
  secureClient_.setBufferSizes(512, 512);
  mqtt_.setCallback(mqttCallback);
  mqtt_.setBufferSize(2048);
  mqtt_.setKeepAlive(30);
  mqtt_.setSocketTimeout(3);
  webSocket_.onEvent(webSocketEvent);
  webSocket_.setReconnectInterval(10000);
  webSocket_.enableHeartbeat(15000, 3000, 2);
  applyConfiguration();
}

String CloudServices::topic(const __FlashStringHelper *suffix) const {
  String value(cloudConfig.mqttBaseTopic());
  if (!value.endsWith("/")) value += '/';
  value += deviceId_;
  value += suffix;
  return value;
}

void CloudServices::applyConfiguration() {
  if (mqtt_.connected()) mqtt_.disconnect();
  webSocket_.disconnect();
  webSocketConfigured_ = false;
  nextMqttAttemptAt_ = 0;
  lastTelemetryAt_ = 0;
  lastError_ = String();
  mqtt_.setServer(cloudConfig.mqttHost(), cloudConfig.mqttPort());
  mqtt_.setClient(cloudConfig.mqttTls()
                      ? static_cast<Client &>(secureClient_)
                      : static_cast<Client &>(plainClient_));
  configureWebSocket();
}

void CloudServices::configureWebSocket() {
  if (!cloudConfig.webSocketEnabled()) return;
  authorizationHeader_ = String();
  if (cloudConfig.webSocketTokenSet()) {
    authorizationHeader_ = F("Authorization: Bearer ");
    authorizationHeader_ += cloudConfig.webSocketToken();
  }
  webSocket_.setExtraHeaders(authorizationHeader_.c_str());
  if (cloudConfig.webSocketTls()) {
    webSocket_.beginSslWithCA(cloudConfig.webSocketHost(),
                              cloudConfig.webSocketPort(),
                              cloudConfig.webSocketPath(), &trustAnchor,
                              "pakarcontroller-v1");
  } else {
    webSocket_.begin(cloudConfig.webSocketHost(),
                     cloudConfig.webSocketPort(),
                     cloudConfig.webSocketPath(), "pakarcontroller-v1");
  }
  webSocketConfigured_ = true;
}

void CloudServices::connectMqtt(uint32_t now) {
  if (!cloudConfig.mqttEnabled() || mqtt_.connected() ||
      static_cast<int32_t>(now - nextMqttAttemptAt_) < 0) {
    return;
  }
  nextMqttAttemptAt_ = now + 10000U;
  const String availability = topic(F("/availability"));
  bool connected = false;
  if (cloudConfig.mqttUser()[0] != '\0') {
    connected = mqtt_.connect(deviceId_.c_str(), cloudConfig.mqttUser(),
                              cloudConfig.mqttPassword(),
                              availability.c_str(), 0, true, "offline");
  } else {
    connected = mqtt_.connect(deviceId_.c_str(), availability.c_str(), 0,
                              true, "offline");
  }
  if (!connected) {
    lastError_ = String(F("MQTT connect failed, state ")) + mqtt_.state();
    return;
  }
  lastError_ = String();
  mqtt_.publish(availability.c_str(), "online", true);
  const String command = topic(F("/command"));
  mqtt_.subscribe(command.c_str());
}

void CloudServices::publishTelemetry(const String &statusJson, uint32_t now) {
  const uint32_t interval =
      static_cast<uint32_t>(cloudConfig.telemetrySeconds()) * 1000U;
  if (static_cast<uint32_t>(now - lastTelemetryAt_) < interval) return;
  lastTelemetryAt_ = now;
  if (mqtt_.connected()) {
    const String statusTopic = topic(F("/status"));
    if (!mqtt_.publish(statusTopic.c_str(), statusJson.c_str(), true)) {
      lastError_ = F("MQTT status publish failed");
    }
  }
  if (webSocket_.isConnected()) {
    webSocket_.sendTXT(statusJson.c_str());
  }
}

void CloudServices::service(bool networkReady, bool timeReady, uint32_t now) {
  if (!networkReady) return;
  if (cloudConfig.mqttEnabled() &&
      (!cloudConfig.mqttTls() || timeReady)) {
    connectMqtt(now);
    mqtt_.loop();
  }
  if (webSocketConfigured_ &&
      (!cloudConfig.webSocketTls() || timeReady)) {
    webSocket_.loop();
  }
  const uint32_t interval =
      static_cast<uint32_t>(cloudConfig.telemetrySeconds()) * 1000U;
  if (statusProvider_ != nullptr &&
      static_cast<uint32_t>(now - lastTelemetryAt_) >= interval) {
    publishTelemetry(statusProvider_(), now);
  }
}

void CloudServices::handleCommand(const uint8_t *payload, size_t length,
                                  bool fromMqtt) {
  if (payload == nullptr || length == 0 || length > 512) {
    lastError_ = F("cloud command length is invalid");
    return;
  }
  String message;
  message.reserve(length);
  message.concat(reinterpret_cast<const char *>(payload), length);
  String result;
  const bool accepted =
      commandHandler_ != nullptr && commandHandler_(message, result);
  String response = F("{\"accepted\":");
  response += accepted ? F("true") : F("false");
  response += F(",\"message\":\"");
  result.replace("\\", "\\\\");
  result.replace("\"", "\\\"");
  response += result;
  response += F("\"}");
  if (fromMqtt && mqtt_.connected()) {
    const String eventTopic = topic(F("/event"));
    mqtt_.publish(eventTopic.c_str(), response.c_str(), false);
  } else if (!fromMqtt && webSocket_.isConnected()) {
    webSocket_.sendTXT(response);
  }
}

void CloudServices::mqttCallback(char *, uint8_t *payload,
                                 unsigned int length) {
  ++cloudServices.mqttMessagesReceived_;
  cloudServices.handleCommand(payload, length, true);
}

void CloudServices::webSocketEvent(WStype_t type, uint8_t *payload,
                                   size_t length) {
  if (type == WStype_CONNECTED) {
    cloudServices.lastError_ = String();
    String hello = F("{\"type\":\"hello\",\"device\":\"");
    hello += cloudServices.deviceId_;
    hello += F("\"}");
    cloudServices.webSocket_.sendTXT(hello);
  } else if (type == WStype_TEXT) {
    ++cloudServices.webSocketMessagesReceived_;
    cloudServices.handleCommand(payload, length, false);
  } else if (type == WStype_ERROR) {
    cloudServices.lastError_ = F("outbound WebSocket error");
  }
}
