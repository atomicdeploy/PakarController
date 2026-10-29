#include "CloudServices.h"
#include <ArduinoJson.h>
#include <BuildInfo.h>
#include <PeerCompatibility.h>
#include <time.h>
#if defined(ESP32)
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#else
#include <libb64/cencode.h>
#include <bearssl/bearssl.h>
extern "C" {
#include <osapi.h>
}
#endif

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

#if !defined(ESP32)
BearSSL::X509List trustAnchor(kIsrgRootX1);
#endif
}  // namespace

void CloudWebSocketClient::serviceExistingConnection() {
  // Actual WebSocketsClient::loop connected branch, deliberately without its
  // allocation/connect branch. A TCP loss can clean up, never reconnect here.
  if (!clientIsConnected(&_client)) return;
  handleClientData();
  yield();
  if (_client.status == WSC_CONNECTED) {
    handleHBPing();
    handleHBTimeout(&_client);
  }
}

CloudServices cloudServices;
class CloudServices::IoScope {
 public:
  IoScope(CloudServices &owner,bool publishing):owner_(owner),publishing_(publishing),startedAt_(millis()) {
    ++owner_.ioDepth_;
    uint32_t &count=publishing?owner_.ioStats_.publishes:owner_.ioStats_.services;
    if(count!=UINT32_MAX)++count;
    if(publishing_)owner_.publishActive_=true;
  }
  ~IoScope() {
    const uint32_t elapsed=static_cast<uint32_t>(millis()-startedAt_);
    uint32_t &last=publishing_?owner_.ioStats_.lastPublishMs:owner_.ioStats_.lastServiceMs;
    uint32_t &maximum=publishing_?owner_.ioStats_.maxPublishMs:owner_.ioStats_.maxServiceMs;
    last=elapsed;if(elapsed>maximum)maximum=elapsed;
    if(publishing_)owner_.publishActive_=false;
    --owner_.ioDepth_;
  }
 private:
  CloudServices &owner_;bool publishing_;uint32_t startedAt_;
};
void CloudServices::cooperate() {
  // yield() lets other RTOS tasks run, not this loop owner's hostLink parser.
  // Keep the callback explicit, synchronous and nonrecursive between writes.
  if(progressService_ && !progressActive_) {
    progressActive_=true;progressService_();progressActive_=false;
  }
  yield();
}
const char *pakarTrustAnchorPem() { return kIsrgRootX1; }
#if !defined(ESP32)
BearSSL::X509List &pakarTrustAnchor() { return trustAnchor; }
#endif

void CloudServices::begin(const String &deviceId, CommandHandler handler,
                          StatusProvider statusProvider,StatusProvider telemetryProvider,
                          const String &boardId,const String &bootId) {
  deviceId_ = deviceId;
  boardId_=boardId;bootId_=bootId;
  commandGuard_.setIdentity(boardId.c_str(),bootId.c_str());
  commandHandler_ = handler;
  statusProvider_ = statusProvider;
  telemetryProvider_=telemetryProvider;
  uint8_t entropy[16];
#if defined(ESP32)
  esp_fill_random(entropy, sizeof(entropy));
  {
#else
  if(os_get_random(entropy,sizeof(entropy))==0) {
#endif
    char identity[33];for(unsigned i=0;i<sizeof(entropy);++i)snprintf(identity+i*2,3,"%02x",entropy[i]);
    telemetryBoot_=identity;
  }
#if defined(ESP32)
  // Arduino ESP32 3.3.12 NetworkClient/NetworkClientSecure share this
  // millisecond TCP-connect setting. It is separate from the TLS handshake
  // limit below; changing neither the CA nor certificate/hostname validation.
  // DNS resolution remains synchronous and is included in measured duration.
  plainClient_.setConnectionTimeout(1000);
  secureClient_.setConnectionTimeout(1000);
  secureClient_.setCACert(pakarTrustAnchorPem());
  secureClient_.setHandshakeTimeout(5);
#else
  secureClient_.setTrustAnchors(&trustAnchor);
  secureClient_.setBufferSizes(512, 512);
#endif
  mqtt_.setCallback(mqttCallback);
  mqtt_.setKeepAlive(30);
  mqtt_.setSocketTimeout(3);
  webSocket_.onEvent(webSocketEvent);
  webSocket_.setReconnectInterval(10000);
  webSocket_.enableHeartbeat(15000, 3000, 2);
  applyConfiguration();
}
void CloudServices::suspend() {
  networkReady_ = false;
  nextMqttAttemptAt_ = 0; // A later route generation must not inherit an aged retry deadline.
  closeMqtt(CloseReason::Suspend);webSocket_.disconnect();
}

void CloudServices::closeMqtt(CloseReason reason) {
  if(mqtt_.connected()) {
    uint32_t &cause=reason==CloseReason::Suspend?ioStats_.suspendCloses:ioStats_.configurationCloses;
    if(cause!=UINT32_MAX)++cause;
    // The current config/name may already have changed. Publish against the
    // exact topic registered with this session's LWT, never the new identity.
    const uint32_t uncleanBefore=ioStats_.uncleanCloses;
    const bool written=availabilityTopic_.length() &&
        publishMqtt(availabilityTopic_,String(F("offline")),true);
    if(written) {
      if(ioStats_.cleanCloses!=UINT32_MAX)++ioStats_.cleanCloses;
      mqtt_.disconnect();
    } else if(ioStats_.uncleanCloses==uncleanBefore && ioStats_.uncleanCloses!=UINT32_MAX)++ioStats_.uncleanCloses;
    // Otherwise drop TCP without MQTT DISCONNECT so the broker can emit LWT.
    // QoS0 write acceptance is not a broker acknowledgement.
  }
  secureClient_.stop();plainClient_.stop();availabilityTopic_=String();
}
void CloudServices::dropMqttWrite() {
  // A partially written packet must close TCP without inserting DISCONNECT
  // into its unfinished payload. Keep this cause separate from route/config.
  if(ioStats_.writeCloses!=UINT32_MAX)++ioStats_.writeCloses;
  if(ioStats_.uncleanCloses!=UINT32_MAX)++ioStats_.uncleanCloses;
  secureClient_.stop();plainClient_.stop();
}

String CloudServices::topic(const __FlashStringHelper *suffix) const {
  String value(cloudConfig.mqttBaseTopic());
  if (!value.endsWith("/")) value += '/';
  value += deviceId_;
  value += suffix;
  return value;
}

void CloudServices::renameDevice(const String &deviceId) {
  if (deviceId_ == deviceId) return;
  deviceId_ = deviceId;
  applyConfiguration();
}

void CloudServices::applyConfiguration() {
  closeMqtt(CloseReason::Configuration);
  webSocket_.disconnect();
  webSocketConfigured_ = false;
  nextMqttAttemptAt_ = 0;
  lastTelemetryAt_ = 0;
  telemetryRequested_ = false;
  lastError_ = String();
  // No enlarged permanent RX buffer on an unused MQTT interface. A configured
  // broker fails closed if the complete atomic-command capacity cannot allocate.
  mqttBufferReady_=!cloudConfig.mqttEnabled() || mqtt_.setBufferSize(kMqttBufferBytes);
  if(!mqttBufferReady_)lastError_=F("MQTT command buffer unavailable; broker connection disabled");
  mqtt_.setServer(cloudConfig.mqttHost(), cloudConfig.mqttPort());
  mqtt_.setClient(cloudConfig.mqttTls()
                      ? static_cast<Client &>(secureClient_)
                      : static_cast<Client &>(plainClient_));
  configureWebSocket();
}

void CloudServices::configureWebSocket() {
  if (!cloudConfig.webSocketEnabled()) return;
  authorizationHeader_ = String("X-Pakar-Client: pakarcontroller-firmware\r\nX-Pakar-Version: ")+BoardCore::BuildInfo::kVersion+
      "\r\nX-Pakar-Client-Id: "+boardId_;
  if (cloudConfig.webSocketTokenSet()) {
    authorizationHeader_ += F("\r\nAuthorization: Bearer ");
    authorizationHeader_ += cloudConfig.webSocketToken();
  }
  webSocket_.setExtraHeaders(authorizationHeader_.c_str());
  if (cloudConfig.webSocketTls()) {
    webSocket_.beginSslWithCA(cloudConfig.webSocketHost(),
                              cloudConfig.webSocketPort(),
                              cloudConfig.webSocketPath(),
#if defined(ESP32)
                              pakarTrustAnchorPem(),
#else
                              &trustAnchor,
#endif
                              "pakarcontroller");
  } else {
    webSocket_.begin(cloudConfig.webSocketHost(),
                     cloudConfig.webSocketPort(),
                     cloudConfig.webSocketPath(), "pakarcontroller");
  }
  webSocketConfigured_ = true;
}

void CloudServices::connectMqtt(uint32_t now) {
  if (!cloudConfig.mqttEnabled() || !mqttBufferReady_ || mqtt_.connected() ||
      (nextMqttAttemptAt_ && static_cast<int32_t>(now - nextMqttAttemptAt_) < 0)) {
    return;
  }
  const char *clientId=BoardCore::PeerFence::boardValid(boardId_.c_str())?
      boardId_.c_str():telemetryBoot_.c_str();
  if(!clientId[0]) {
    lastError_=F("Unique MQTT client identity unavailable");
    nextMqttAttemptAt_=millis()+10000U;if(!nextMqttAttemptAt_)nextMqttAttemptAt_=1;
    return;
  }
  const String availability = topic(F("/availability"));
  cooperate();
#if defined(ESP32)
  const bool tlsAttempt=cloudConfig.mqttTls();
  constexpr uint32_t internalCaps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
  const size_t internalFreeBefore=tlsAttempt?heap_caps_get_free_size(internalCaps):0;
  const size_t internalLargestBefore=tlsAttempt?heap_caps_get_largest_free_block(internalCaps):0;
#endif
  const uint32_t startedAt=millis();
  if(mqttConnectionStats_.attempts!=UINT32_MAX)++mqttConnectionStats_.attempts;
  bool connected = false;
  if (cloudConfig.mqttUser()[0] != '\0') {
    connected = mqtt_.connect(clientId, cloudConfig.mqttUser(),
                              cloudConfig.mqttPassword(),
                              availability.c_str(), 0, true, "offline");
  } else {
    connected = mqtt_.connect(clientId, availability.c_str(), 0,
                              true, "offline");
  }
  const uint32_t completedAt=millis();
  mqttConnectionStats_.lastDurationMs=static_cast<uint32_t>(completedAt-startedAt);
  if(mqttConnectionStats_.lastDurationMs>mqttConnectionStats_.maxDurationMs)
    mqttConnectionStats_.maxDurationMs=mqttConnectionStats_.lastDurationMs;
  // Schedule from completion, not stale loop-entry `now`: DNS/TCP/TLS may have
  // consumed the whole interval already. Never immediately reenter a failed
  // blocking attempt. Reserve zero as the existing immediate/reconfigured flag.
  nextMqttAttemptAt_=completedAt+10000U;if(!nextMqttAttemptAt_)nextMqttAttemptAt_=1;
  if (!connected) {
    if(mqttConnectionStats_.failures!=UINT32_MAX)++mqttConnectionStats_.failures;
#if defined(ESP32)
    // PubSubClient -2 means its Client::connect failed, not MQTT credential
    // rejection. Retain the SDK's actual TLS result before any later socket
    // work; a zero result is not proof of a certificate or memory failure.
    if(tlsAttempt && mqtt_.state()==-2) {
      const size_t internalFreeAfter=heap_caps_get_free_size(internalCaps);
      const size_t internalLargestAfter=heap_caps_get_largest_free_block(internalCaps);
      char reason[96]={};
      const int tlsError=secureClient_.lastError(reason,sizeof(reason));
      reason[sizeof(reason)-1]='\0';
      for(size_t i=0;reason[i];++i)
        if(static_cast<unsigned char>(reason[i])<32 || static_cast<unsigned char>(reason[i])>126)reason[i]=' ';
      char diagnostic[320];
      snprintf(diagnostic,sizeof(diagnostic),
          "MQTT connect failed, state %d; tls_error %d (%s); elapsed_ms %lu; internal_free %lu->%lu; internal_largest %lu->%lu",
          mqtt_.state(),tlsError,reason[0]?reason:"no SDK detail",
          static_cast<unsigned long>(mqttConnectionStats_.lastDurationMs),
          static_cast<unsigned long>(internalFreeBefore),static_cast<unsigned long>(internalFreeAfter),
          static_cast<unsigned long>(internalLargestBefore),static_cast<unsigned long>(internalLargestAfter));
      lastError_=diagnostic;
    } else
#endif
    lastError_ = String(F("MQTT connect failed, state ")) + mqtt_.state();
    cooperate();
    return;
  }
  cooperate();
  availabilityTopic_=availability;
  if(availabilityTopic_.length()!=availability.length()) {
    secureClient_.stop();plainClient_.stop();availabilityTopic_=String();
    lastError_=F("MQTT availability identity allocation failed");return;
  }
  lastError_ = String();
  const String command = topic(F("/command"));
  if(!mqtt_.subscribe(command.c_str())) {
    lastError_=F("MQTT command subscription write failed; connection closed");
    dropMqttWrite();cooperate();return;
  }
  // PubSubClient exposes write acceptance, not SUBACK/PUBACK confirmation.
  // Publish availability only after the subscription packet has been written.
  if(!publishMqtt(availability,String(F("online")),true)) {
    lastError_=F("MQTT availability write failed; connection closed");return;
  }
  requestTelemetry();
  cooperate();
}

bool CloudServices::publishMqtt(const String &destination,const String &json,bool retained) {
  // PubSubClient's publish() buffers the entire payload. beginPublish streams a
  // bounded JSON snapshot without increasing the shared 2048-byte RX buffer.
  // Bounded outbound receipts may include an escaped LAN peer response. This
  // does not enlarge MQTT RX/commands or the separately bounded status stream.
  if (publishActive_ || progressActive_ || !mqtt_.connected() || json.length()>32768 || destination.length()>192) return false;
  IoScope io(*this,true);
  cooperate();
  if (!mqtt_.beginPublish(destination.c_str(),json.length(),retained)) {
    dropMqttWrite();cooperate();return false;
  }
  cooperate();
  for(size_t offset=0;offset<json.length();offset+=256) {
    const size_t bytes=min<size_t>(256,json.length()-offset);
    if(mqtt_.write(reinterpret_cast<const uint8_t*>(json.c_str()+offset),bytes)!=bytes) {
      // A partially written PUBLISH cannot be repaired with another packet or
      // replayed. Drop TCP directly; MQTT DISCONNECT would corrupt its payload.
      dropMqttWrite();cooperate();return false;
    }
    cooperate();
  }
  // Upstream endPublish() is not a broker PUBACK (QoS0); this is write evidence.
  const bool written=mqtt_.endPublish()==1;
  cooperate();return written;
}

void CloudServices::publishTelemetry(String statusJson, uint32_t now,bool includeFullTelemetry) {
  const uint32_t interval =
      static_cast<uint32_t>(cloudConfig.telemetrySeconds()) * 1000U;
  if (!telemetryRequested_ && static_cast<uint32_t>(now - lastTelemetryAt_) < interval) return;
  telemetryRequested_=false;
  lastTelemetryAt_ = now;
  const bool statusReady=statusJson.length()>0 && statusJson.length()<=16384;
  if(!statusReady) lastError_=F("Status exceeds bounded snapshot size or is empty");
  if (statusReady && mqtt_.connected()) {
    const String statusTopic = topic(F("/status"));
    if (!publishMqtt(statusTopic,statusJson,true)) {
      lastError_ = F("MQTT status publish failed");
    }
  }
  if (statusReady && webSocket_.isConnected()) {
    cooperate();
    webSocket_.sendTXT(statusJson.c_str());
    cooperate();
  }
  // Own the provider's temporary and release it BEFORE constructing the larger
  // aggregate. Retaining both snapshots exhausted internal RAM alongside TLS.
  statusJson=String();
  if(includeFullTelemetry && telemetryProvider_ && (mqtt_.connected() || webSocket_.isConnected())) {
    const String full=telemetryProvider_();
    if(full.length()>kTelemetryBytes) {lastError_=F("Telemetry exceeds bounded snapshot size");return;}
    if(webSocket_.isConnected()) {cooperate();webSocket_.sendTXT(full.c_str());cooperate();}
    if(mqtt_.connected() && telemetryBoot_.length()) publishTelemetryFragments(full);
  }
}

void __attribute__((noinline)) CloudServices::publishTelemetryFragments(const String &full) {
  // Preserve donor /status consumers. Full snapshots use bounded packets,
  // not a heap-heavy enlarged MQTT buffer. Reassemble only matching complete
  // snapshot IDs, then verify SHA-256 before parsing the JSON.
  uint8_t bytes[32];char hex[65];
#if defined(ESP32)
  if (mbedtls_sha256(reinterpret_cast<const unsigned char *>(full.c_str()), full.length(), bytes, 0) != 0) {
    lastError_ = F("Telemetry integrity hashing failed"); return;
  }
#else
  br_sha256_context hash;br_sha256_init(&hash);br_sha256_update(&hash,full.c_str(),full.length());
  br_sha256_out(&hash,bytes);
#endif
  for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",bytes[i]);
  const unsigned count=(full.length()+767)/768;
  const String id=telemetryBoot_+"-"+String(++snapshotSequence_);
  const String destination=topic(F("/telemetry/part"));
  char encoded[1060]; // Reused only after the full snapshot builder has returned.
  for(unsigned i=0;i<count;++i) {
    const unsigned n=min<unsigned>(768,full.length()-i*768);
#if defined(ESP32)
    size_t size = 0;
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char *>(encoded), sizeof(encoded), &size,
        reinterpret_cast<const unsigned char *>(full.c_str() + i * 768), n) != 0) {
      lastError_ = F("Telemetry encoding failed"); break;
    }
    encoded[size] = 0;
#else
    base64_encodestate state;base64_init_encodestate(&state);
    int size=base64_encode_block(full.c_str()+i*768,n,encoded,&state);
    size+=base64_encode_blockend(encoded+size,&state);encoded[size]=0;
#endif
    JsonDocument part;part["type"]="telemetry-fragment";part["snapshot_id"]=id;
    part["index"]=i;part["count"]=count;part["total_bytes"]=full.length();
    part["sha256"]=hex;part["encoding"]="base64";part["data"]=encoded;
    String packet;serializeJson(part,packet);
    cooperate();
    const bool written=mqtt_.publish(destination.c_str(),packet.c_str(),false);
    if(!written) {dropMqttWrite();lastError_=F("Telemetry fragment publish failed");cooperate();break;}
    cooperate();
  }
}

void CloudServices::service(bool networkReady, bool timeReady, uint32_t now,
                            bool allowConnect,bool includeFullTelemetry) {
  if(ioActive() || progressActive_)return;
  IoScope io(*this,false);
  if (!networkReady) {
    if (networkReady_) suspend(); // Drop stale cloud sessions once per link loss.
    return;
  }
  networkReady_ = true;
  if (cloudConfig.mqttEnabled() &&
      (!cloudConfig.mqttTls() || timeReady)) {
    if(allowConnect) connectMqtt(now);
    if(mqtt_.connected()) {cooperate();mqtt_.loop();cooperate();}
  }
  if (webSocketConfigured_ &&
      (!cloudConfig.webSocketTls() || timeReady)) {
    cooperate();
    if(allowConnect) webSocket_.loop();
    else webSocket_.serviceExistingConnection();
    cooperate();
  }
  const uint32_t interval =
      static_cast<uint32_t>(cloudConfig.telemetrySeconds()) * 1000U;
  if (statusProvider_ != nullptr && (mqtt_.connected() || webSocket_.isConnected()) &&
      (telemetryRequested_ || static_cast<uint32_t>(now - lastTelemetryAt_) >= interval)) {
    publishTelemetry(statusProvider_(), now,includeFullTelemetry);
  }
}

void CloudServices::handleCommand(const uint8_t *payload, size_t length,
                                  bool fromMqtt) {
  if (payload == nullptr || length == 0 || length > kCommandBytes || memchr(payload,0,length)) {
    lastError_ = F("cloud command length is invalid");
    commandReply(fromMqtt,"invalid_json",nullptr,false,lastError_);
    return;
  }
  JsonDocument document;
  if(deserializeJson(document,payload,length) || !document.is<JsonObject>() ||
     !document["command"].is<const char*>()) {
    lastError_=F("Cloud command must be a JSON object with a command string");
    commandReply(fromMqtt,"invalid_json",nullptr,false,lastError_);return;
  }
  auto wholeString=[&document](const char *key)->const char* {
    if(!document[key].is<const char*>())return nullptr;
    const JsonString value=document[key].as<JsonString>();
    return value.c_str() && strlen(value.c_str())==value.size()?value.c_str():nullptr;
  };
  const char *command=wholeString("command"),*requestId=wholeString("request_id");
  const char *replyId=requestId && strlen(requestId)<=40?requestId:nullptr;
  const bool readOnly=BoardCore::CloudCommand::Guard::readOnly(command);
  if(!command || (!readOnly && (!wholeString("board_id") || !wholeString("boot_id") ||
      !requestId || !document["expires_at"].is<uint32_t>()))) {
    lastError_=F("Mutation requires typed board_id, boot_id, request_id and expires_at UTC seconds");
    commandReply(fromMqtt,"invalid_envelope",replyId,false,lastError_);return;
  }
  const time_t clock=time(nullptr);
  const uint32_t epoch=clock>=1700000000 && static_cast<uint64_t>(clock)<=UINT32_MAX?
      static_cast<uint32_t>(clock):0;
  const auto admission=commandGuard_.admit(command,wholeString("board_id"),wholeString("boot_id"),
      requestId,document["expires_at"]|uint32_t(0),epoch,millis());
  if(admission!=BoardCore::CloudCommand::Result::Allowed) {
    lastError_=String(F("Cloud command rejected: "))+BoardCore::CloudCommand::code(admission);
    commandReply(fromMqtt,BoardCore::CloudCommand::code(admission),replyId,false,lastError_);return;
  }
  if((!readOnly||!document["client_version"].isNull()) &&
      !BoardCore::PeerCompatibility::appAllowed(wholeString("client_version"))) {
    lastError_=F("Unsupported app version; inspect the advertised minimum_app release policy");
    commandReply(fromMqtt,"unsupported_app_version",replyId,false,lastError_);return;
  }
  // ID already consumed. A lost reply, handler rejection or duplicate arriving
  // through the other cloud channel cannot dispatch the mutation a second time.
  String message;
  const size_t normalizedBytes=measureJson(document);
  if(serializeJson(document,message)!=normalizedBytes || message.length()!=normalizedBytes) {
    lastError_=F("Cloud JSON normalization failed; nothing dispatched or replayed");
    commandReply(fromMqtt,"normalization_failed",replyId,false,lastError_);return;
  }
  // The existing handler has lightweight string helpers. Forward the SAME
  // parsed document, never ambiguous duplicate keys/escaped raw key spellings
  // that those helpers could interpret differently from this admission policy.
  String result;
  const bool accepted =
      commandHandler_ != nullptr && commandHandler_(message, result);
  const char *failure=result==F("red_cells_locked")?"red_cells_locked":result==F("invalid_cell_assignments")?
    "invalid_cell_assignments":result==F("unsupported_board_role")?"unsupported_board_role":"handler_rejected";
  commandReply(fromMqtt,accepted?"accepted":failure,replyId,accepted,result);
  // The result acknowledges handler admission, not optical/peripheral success.
  // A following status reflects the actual owner state, not a request echo.
  if(accepted)requestTelemetry();
}

void CloudServices::commandReply(bool fromMqtt,const char *code,const char *requestId,
                                  bool accepted,const String &message) {
  JsonDocument reply;reply["type"]="command-result";
  reply["board_id"]=boardId_;reply["boot_id"]=bootId_;
  reply["accepted"]=accepted;reply["message"]=message;
  reply["code"]=code;reply["request_id"]=requestId;
  if(!accepted)reply["error"]=code;
  String response;serializeJson(reply,response);
  if (fromMqtt && mqtt_.connected()) {
    const String eventTopic = topic(F("/event"));
    if(!publishMqtt(eventTopic,response,false))lastError_=F("MQTT command reply write failed; not replayed");
  } else if (!fromMqtt && webSocket_.isConnected()) {
    if(!webSocket_.sendTXT(response))lastError_=F("WebSocket command reply write failed; not replayed");
  }
}

void CloudServices::mqttCallback(char *, uint8_t *payload,
                                 unsigned int length) {
  ++cloudServices.mqttMessagesReceived_;
  cloudServices.handleCommand(payload, length, true);
}

void CloudServices::publishEvent(const String &event) {
  if(progressActive_ || publishActive_) {lastError_=F("Recursive cloud event publication refused; not replayed");return;}
  if (mqtt_.connected()) {
    const String destination = topic(F("/event"));
    if(!publishMqtt(destination,event,false))lastError_=F("MQTT event write failed; not replayed");
  }
  if (webSocket_.isConnected() && !webSocket_.sendTXT(event.c_str()))
    lastError_=F("WebSocket event write failed; not replayed");
}

void CloudServices::webSocketEvent(WStype_t type, uint8_t *payload,
                                   size_t length) {
  if (type == WStype_CONNECTED) {
    // A healthy second transport must not erase a failed MQTT subscription or
    // status write from this same service pass. Clear only this route's fault.
    if(cloudServices.lastError_==F("outbound WebSocket error") ||
        strncmp(cloudServices.lastError_.c_str(),"WebSocket ",10)==0)
      cloudServices.lastError_=String();
    JsonDocument identity;identity["type"]="hello";identity["device"]=cloudServices.deviceId_;
    identity["board_id"]=cloudServices.boardId_;identity["boot_id"]=cloudServices.bootId_;
    identity["command_fencing"]=BoardCore::PeerFence::ready(cloudServices.boardId_.c_str(),cloudServices.bootId_.c_str());
    identity["command_envelope"]=F("board_id,boot_id,request_id,expires_at");
    identity["expires_at_unit"]=F("unix_seconds");identity["maximum_command_lifetime_seconds"]=60;
    identity["command_capacity"]=BoardCore::CloudCommand::Guard::kCapacity;
    String hello;serializeJson(identity,hello);
    cloudServices.webSocket_.sendTXT(hello);
    cloudServices.requestTelemetry();
  } else if (type == WStype_TEXT) {
    ++cloudServices.webSocketMessagesReceived_;
    cloudServices.handleCommand(payload, length, false);
  } else if (type == WStype_ERROR) {
    cloudServices.lastError_ = F("outbound WebSocket error");
  }
}
