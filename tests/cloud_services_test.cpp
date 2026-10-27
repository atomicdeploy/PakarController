#include <assert.h>
#include <time.h>
#include <ArduinoJson.h>
#include "CloudServices.h"
#include <filesystem>
#include <fstream>
#include <iterator>

static unsigned dispatches=0,fullSnapshots=0;
static String dispatched;
static String statusJson=String("{\"data\":\"")+std::string(6000,'x')+"\"}";
static String fullJson="{\"full\":true}";
static bool handler(const String &message,String &out){++dispatches;dispatched=message;out="submitted";return true;}
static String status(){return statusJson;}
static String full(){++fullSnapshots;return fullJson;}
static const char *board="esp32s3-907069eac800",*boot="123456789abcdef0123456789abcdef0";
static String envelope(const char *id,const char *command="cell") {
  JsonDocument d;d["command"]=command;d["board_id"]=board;d["boot_id"]=boot;
  d["client_version"]="0.2.8";
  d["request_id"]=id;d["expires_at"]=static_cast<uint32_t>(time(nullptr))+60;
  String out;serializeJson(d,out);return out;
}
static JsonDocument lastReply(){JsonDocument d;assert(!deserializeJson(d,WebSocketsClient::latest->sent.back()));return d;}
static void service(bool networkReady,bool timeReady,uint32_t now,bool allowConnect=true,bool includeFull=true){
  cloudTestMillis=now;cloudServices.service(networkReady,timeReady,now,allowConnect,includeFull);
}
static std::vector<unsigned> progressWrites;
static void progress(){
  assert(cloudServices.ioActive());
  auto &mqtt=*PubSubClient::latest;auto &ws=*WebSocketsClient::latest;
  const auto services=cloudServices.ioStats().services,publishes=cloudServices.ioStats().publishes;
  const auto packets=mqtt.packets.size(),texts=ws.sent.size();
  progressWrites.push_back(mqtt.writes);
  // A bad callback cannot recursively run a cloud socket or insert an MQTT
  // event into a half-written status packet. Production callback is read-only.
  cloudServices.service(true,true,cloudTestMillis,false,false);
  cloudServices.publishEvent("{\"recursive\":true}");
  assert(cloudServices.ioStats().services==services&&cloudServices.ioStats().publishes==publishes);
  assert(mqtt.packets.size()==packets&&ws.sent.size()==texts);
}
static void fragmentStackBoundary(){
  // Prevent accidentally merging the1KB scratch back into the full telemetry
  // builder's call chain. This guards source placement; real S3 stack frames
  // must still be checked in the compiled ELF and on the actual loop task.
  const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
  const auto read=[&](const char* path){
    std::ifstream input(root/path);assert(input.good());
    return std::string(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());
  };
  const auto source=read("firmware/esp/src/CloudServices.cpp");
  const auto header=read("firmware/esp/src/CloudServices.h");
  const auto caller=source.find("void CloudServices::publishTelemetry(");
  const auto helper=source.find("void __attribute__((noinline)) CloudServices::publishTelemetryFragments(");
  const auto serviceAt=source.find("void CloudServices::service(",helper);
  assert(caller!=std::string::npos&&helper>caller&&serviceAt>helper);
  const auto buildScope=source.substr(caller,helper-caller),fragmentScope=source.substr(helper,serviceAt-helper);
  assert(buildScope.find("telemetryProvider_()")!=std::string::npos);
  assert(buildScope.find("publishTelemetryFragments(full)")!=std::string::npos);
  assert(buildScope.find("char encoded[1060]")==std::string::npos);
  assert(buildScope.find("uint8_t bytes[32]")==std::string::npos);
  assert(fragmentScope.find("char encoded[1060]")!=std::string::npos);
  assert(fragmentScope.find("telemetryProvider_()")==std::string::npos);
  assert(header.find("publishTelemetryFragments(const String &full) __attribute__((noinline))")!=std::string::npos);
}
int main(){
  fragmentStackBoundary();
  fs::FS filesystem;assert(cloudConfig.begin(filesystem));
  assert(cloudConfig.setMqtt(true,false,"broker.local",1883,"","","pakar",true));
  assert(cloudConfig.setWebSocket(true,false,"cloud.local",80,"/ws","",true));
  assert(cloudConfig.setTelemetrySeconds(3600)); // First snapshot must not wait an hour.
  cloudServices.begin("ledboard",handler,status,full,board,boot);
  auto &mqtt=*PubSubClient::latest;auto &ws=*WebSocketsClient::latest;
  assert(mqtt.bufferSize==CloudServices::kMqttBufferBytes);
  service(true,true,1000);
  assert(mqtt.connectAttempts==1&&ws.connectAttempts==1&&fullSnapshots==1);
  assert(cloudConfig.setTelemetrySeconds(1));
#if defined(ESP32)
  assert(mqtt.client->connectionTimeoutMs==1000); // Plain TCP is bounded independently of TLS.
#else
  assert(mqtt.client->connectionTimeoutMs==0); // ESP8266 timeout policy is unchanged.
#endif
  assert(mqtt.clientIds.back()==board); // Human hostnames are not unique broker client IDs.
  bool large=false;
  for(const auto &p:mqtt.packets)if(p.topic.endsWith("/status")){assert(p.payload==statusJson&&p.retained);large=true;}
  assert(large&&mqtt.largestWrite<=256&&mqtt.beginCalls==mqtt.endCalls);
  JsonDocument hello;assert(!deserializeJson(hello,ws.sent.front()));
  assert(hello["board_id"]==board&&hello["boot_id"]==boot&&hello["command_fencing"]==true);
  assert(hello["command_capacity"]==BoardCore::CloudCommand::Guard::kCapacity&&hello["expires_at_unit"]=="unix_seconds");

  ws.inject("{\"command\":\"cell\"}");assert(dispatches==0);
  assert(lastReply()["code"]=="invalid_envelope");
  assert(lastReply()["type"]=="command-result"&&lastReply()["board_id"]==board&&lastReply()["boot_id"]==boot);
  assert(lastReply()["error"]=="invalid_envelope");
  ws.inject("{\"command\":\"status\"}");assert(dispatches==1&&lastReply()["accepted"]==true);
  auto command=envelope("once");ws.inject(command);assert(dispatches==2);
  assert(lastReply()["request_id"]=="once");
  assert(lastReply()["type"]=="command-result"&&lastReply()["board_id"]==board&&lastReply()["boot_id"]==boot);
  assert(lastReply()["error"].isNull());
  JsonDocument unsupported;assert(!deserializeJson(unsupported,envelope("unsupported-app")));
  unsupported["client_version"]="0.2.6";String invalidRelease;serializeJson(unsupported,invalidRelease);
  const unsigned beforeUnsupported=dispatches;ws.inject(invalidRelease);
  assert(dispatches==beforeUnsupported&&lastReply()["code"]=="unsupported_app_version");
  mqtt.inject(command);assert(dispatches==2); // One cache for both channels.
  JsonDocument duplicate;assert(!deserializeJson(duplicate,mqtt.packets.back().payload));
  assert(duplicate["accepted"]==false&&duplicate["code"]=="duplicate"&&duplicate["request_id"]=="once");
  assert(duplicate["type"]=="command-result"&&duplicate["board_id"]==board&&duplicate["boot_id"]==boot&&duplicate["error"]=="duplicate");
  JsonDocument bad;assert(!deserializeJson(bad,envelope("bad-type")));bad["expires_at"]="9999999999";
  String encoded;serializeJson(bad,encoded);ws.inject(encoded);assert(dispatches==2);
  assert(lastReply()["code"]=="invalid_envelope");
  assert(!deserializeJson(bad,envelope("stale")));bad["expires_at"]=static_cast<uint32_t>(time(nullptr))-1;
  encoded.clear();serializeJson(bad,encoded);ws.inject(encoded);assert(dispatches==2&&lastReply()["code"]=="expired");
  ws.inject("[]");assert(lastReply()["code"]=="invalid_json");
  // ArduinoJson's last duplicate key wins. The lightweight legacy handler must
  // receive that same canonical object, not see an earlier unfenced mutation.
  ws.inject("{\"command\":\"cell\",\"command\":\"status\",\"id\":1}");
  assert(dispatches==3&&dispatched=="{\"command\":\"status\",\"id\":1}");
  ws.inject("{\"command\":\"status\",\"command\":\"cell\"}");
  assert(dispatches==3&&lastReply()["code"]=="invalid_envelope");
  ws.inject("{\"command\":\"status\\u0000cell\"}");
  assert(dispatches==3&&lastReply()["code"]=="invalid_envelope");

  // Admission ACK is small/correlated; its owner-state readback is coalesced
  // and built only after the receive callback returns, before the timer is due.
  const auto readbacks=fullSnapshots;
  service(true,true,1001,false,true);
  assert(fullSnapshots==readbacks+1&&ws.sent.back()==fullJson);
  const auto writtenStatus=mqtt.packets.size();
  service(true,true,1002,false,false);
  assert(mqtt.packets.size()==writtenStatus); // No per-loop refresh storm.

  const auto previousFull=fullSnapshots,previousMqtt=mqtt.connectAttempts,previousWs=ws.connectAttempts;
  service(true,true,2000,false,false);
  assert(fullSnapshots==previousFull&&mqtt.connectAttempts==previousMqtt&&ws.connectAttempts==previousWs);
  assert(mqtt.loops>=2&&ws.receives>0&&ws.pings>0&&ws.timeouts>0);
  ws.loseTcp();service(true,true,3000,false,false);
  assert(ws.connectAttempts==previousWs&&!ws.isConnected());
  cloudServices.suspend();service(true,true,4000,false,false);
  assert(mqtt.connectAttempts==previousMqtt&&ws.connectAttempts==previousWs);
  assert(mqtt.packets.back().topic=="pakar/ledboard/availability"&&mqtt.packets.back().payload=="offline"&&mqtt.packets.back().retained);
  service(true,true,12000,true,false);
  assert(mqtt.connectAttempts==previousMqtt+1&&ws.connectAttempts==previousWs+1);
  // A short stream is closed, never finished/replayed over the damaged session.
  const auto ends=mqtt.endCalls;mqtt.shortWriteLimit=20;
  service(true,true,13000,false,false);
  assert(mqtt.endCalls==ends&&!mqtt.connected());
  // A zero reconnect sentinel is immediate even after the signed-half-range of
  // millis uptime. Initial/reconfigured connections must not stall for weeks.
  mqtt.shortWriteLimit=SIZE_MAX;cloudServices.suspend();
  constexpr uint32_t highUptime=0x90000000U;
  service(true,true,highUptime,true,false);
  assert(mqtt.connected()&&mqtt.clientIds.back()==board);
  cloudServices.applyConfiguration();
  service(true,true,highUptime+1,true,false);
  assert(mqtt.connected());
  auto disconnects=mqtt.disconnects;
  cloudServices.renameDevice("renamed");
  assert(mqtt.disconnects==disconnects+1);
  assert(mqtt.packets.back().topic=="pakar/ledboard/availability"&&mqtt.packets.back().payload=="offline");
  service(true,true,highUptime+1000,true,false);assert(mqtt.connected());
  assert(cloudConfig.setMqtt(true,false,"broker.local",1883,"","","changed",false));
  cloudServices.applyConfiguration();
  assert(mqtt.packets.back().topic=="pakar/renamed/availability"&&mqtt.packets.back().payload=="offline");
  service(true,true,highUptime+2000,true,false);assert(mqtt.connected());
  service(false,true,highUptime+3000,false,false);
  assert(mqtt.packets.back().topic=="changed/renamed/availability"&&mqtt.packets.back().payload=="offline");
  service(true,true,highUptime+20000,true,false);assert(mqtt.connected());
  disconnects=mqtt.disconnects;mqtt.shortWriteLimit=0;
  cloudServices.suspend();
  assert(!mqtt.connected()&&mqtt.disconnects==disconnects); // Failed offline write leaves LWT possible.
  mqtt.shortWriteLimit=SIZE_MAX;
  cloudServices.begin("ledboard",handler,status,full,"invalid\nboard","invalid-boot");
  service(true,true,highUptime+21000,true,false);
  assert(mqtt.connected()&&mqtt.clientIds.back()!="ledboard"&&mqtt.clientIds.back().size()==32);
  // A slow failed connect must wait a full interval AFTER completion, not
  // immediately retry because its loop-entry timestamp is twenty seconds old.
  cloudServices.suspend();mqtt.connectResult=false;mqtt.connectDurationMs=20000;
  const auto attempts=cloudServices.mqttConnectionStats().attempts;
  const auto failures=cloudServices.mqttConnectionStats().failures;
  service(true,true,100000,true,false);
  assert(cloudTestMillis==120000);
  auto stats=cloudServices.mqttConnectionStats();
  assert(stats.attempts==attempts+1&&stats.failures==failures+1);
  assert(stats.lastDurationMs==20000&&stats.maxDurationMs==20000);
  service(true,true,120001,true,false);
  service(true,true,119000,true,false); // An older caller timestamp cannot bypass backoff.
  service(true,true,129999,true,false);
  assert(cloudServices.mqttConnectionStats().attempts==attempts+1);
  mqtt.connectResult=true;mqtt.connectDurationMs=5;
  service(true,true,130000,true,false);
  stats=cloudServices.mqttConnectionStats();
  assert(mqtt.connected()&&stats.attempts==attempts+2&&stats.failures==failures+1);
  assert(stats.lastDurationMs==5&&stats.maxDurationMs==20000);
  // Same completion-relative delay and elapsed arithmetic across millis wrap.
  cloudServices.suspend();mqtt.connectResult=false;mqtt.connectDurationMs=20000;
  constexpr uint32_t wrapStart=0xfffffff0U,wrapComplete=wrapStart+20000U;
  service(true,true,wrapStart,true,false);
  stats=cloudServices.mqttConnectionStats();assert(stats.lastDurationMs==20000);
  const auto wrapAttempts=stats.attempts;
  service(true,true,wrapComplete+9999U,true,false);
  assert(cloudServices.mqttConnectionStats().attempts==wrapAttempts);
  mqtt.connectDurationMs=0;service(true,true,wrapComplete+10000U,true,false);
  assert(cloudServices.mqttConnectionStats().attempts==wrapAttempts+1);
#if defined(ESP32)
  assert(cloudConfig.setMqtt(true,true,"broker.local",8883,"","","pakar",true));
  cloudServices.applyConfiguration();mqtt.connectResult=true;
  service(true,true,100000,true,false);
  assert(mqtt.connected()&&mqtt.client->connectionTimeoutMs==1000); // TLS client's TCP limit also set.
#endif
  mqtt.connectResult=true;mqtt.connectDurationMs=0;mqtt.writeDurationMs=0;
  cloudServices.suspend();service(true,true,200000,true,false);assert(mqtt.connected());
  const auto cleanBefore=cloudServices.ioStats();
  cloudServices.suspend();
  auto io=cloudServices.ioStats();
  assert(io.suspendCloses==cleanBefore.suspendCloses+1&&io.cleanCloses==cleanBefore.cleanCloses+1);
  service(true,true,200001,true,false);assert(mqtt.connected());
  const auto configurationBefore=cloudServices.ioStats();cloudServices.applyConfiguration();
  io=cloudServices.ioStats();
  assert(io.configurationCloses==configurationBefore.configurationCloses+1&&io.cleanCloses==configurationBefore.cleanCloses+1);
  service(true,true,200002,true,false);assert(mqtt.connected());
  // Cooperate BETWEEN every real payload write, not merely yield to a different
  // RTOS task after sending the whole6KB document. Total duration is measured.
  const auto writesBefore=mqtt.writes;
  mqtt.writeDurationMs=7;cloudServices.setProgressService(progress);
  service(true,true,201002,false,false);
  const auto payloadWrites=mqtt.writes-writesBefore;
  assert(payloadWrites==(statusJson.length()+255)/256);
  assert(progressWrites.size()>payloadWrites);
  unsigned previous=writesBefore;
  for(const auto written:progressWrites){assert(written-previous<=1);previous=written;}
  io=cloudServices.ioStats();
  assert(!cloudServices.ioActive()&&io.lastPublishMs==payloadWrites*7&&io.lastServiceMs==payloadWrites*7);
  assert(io.maxPublishMs>=io.lastPublishMs&&io.maxServiceMs>=io.lastServiceMs);
  // A short write still closes without endPublish/replay, and services pending
  // read-only progress on the failure path while the IO guard remains active.
  const auto failedBefore=cloudServices.ioStats();const auto endsBefore=mqtt.endCalls;
  const auto failedWrites=mqtt.writes;progressWrites.clear();mqtt.shortWriteLimit=20;
  service(true,true,202002,false,false);
  io=cloudServices.ioStats();
  assert(!mqtt.connected()&&!cloudServices.ioActive()&&mqtt.writes==failedWrites+1&&mqtt.endCalls==endsBefore);
  assert(io.writeCloses==failedBefore.writeCloses+1&&io.uncleanCloses==failedBefore.uncleanCloses+1);
  assert(io.lastPublishMs==7&&progressWrites.size()>=3);
  cloudServices.setProgressService(nullptr);mqtt.shortWriteLimit=SIZE_MAX;mqtt.writeDurationMs=0;
  cloudServices.suspend();service(true,true,203002,true,false);assert(mqtt.connected());
  const auto fragmentBefore=cloudServices.ioStats();mqtt.publishResult=false;
  service(true,true,204002,false,true);
  io=cloudServices.ioStats();
  assert(!mqtt.connected()&&io.writeCloses==fragmentBefore.writeCloses+1);
  assert(io.uncleanCloses==fragmentBefore.uncleanCloses+1);
  mqtt.publishResult=true;
  // Fragment helper preserves the exact serialized envelope and reuses the
  // same bounded scratch for two full chunks and the short tail. Primitive
  // doubles output 'A'/zero digest; this is not crypto/base64 implementation proof.
  fullJson=String("{\"data\":\"")+std::string(2101,'z')+"\"}";
  cloudServices.suspend();service(true,true,205002,true,false);assert(mqtt.connected());
  const auto packetStart=mqtt.packets.size();
  service(true,true,206002,false,true);
  unsigned fragments=0;String snapshotId;
  for(size_t i=packetStart;i<mqtt.packets.size();++i){
    const auto& packet=mqtt.packets[i];if(!packet.topic.endsWith("/telemetry/part"))continue;
    JsonDocument part;assert(!deserializeJson(part,packet.payload));
    if(!fragments)snapshotId=part["snapshot_id"].as<const char*>();
    const size_t bytes=min<size_t>(768,fullJson.length()-fragments*768);
    JsonDocument expected;
    expected["type"]="telemetry-fragment";expected["snapshot_id"]=snapshotId;
    expected["index"]=fragments;expected["count"]=3;expected["total_bytes"]=fullJson.length();
    expected["sha256"]=String(std::string(64,'0'));expected["encoding"]="base64";
    expected["data"]=String(std::string((bytes+2)/3*4,'A'));
    String wire;serializeJson(expected,wire);
    assert(packet.payload==wire&&!packet.retained);++fragments;
  }
  assert(fragments==3);
  cloudServices.begin("ledboard",handler,status,full,board,boot);service(true,true,250000,true,false);
  JsonDocument assignmentEnvelope;assert(!deserializeJson(assignmentEnvelope,envelope("bulk-import-test","cell-assignments")));
  for(unsigned cell=1;cell<=80;++cell){auto row=assignmentEnvelope["assignments"].add<JsonObject>();row["cell"]=cell;row["state"]="white";}
  String import;serializeJson(assignmentEnvelope,import);assert(import.length()>2048&&import.length()<CloudServices::kCommandBytes);
  const auto importDispatches=dispatches;mqtt.inject(import);assert(dispatches==importDispatches+1);
  // Outbound events and inbound commands share the same live session. No
  // mutation is replayed after a failed result/event write or reconnect.
  const String event="{\"type\":\"panel-event\",\"board_id\":\"esp32s3-907069eac800\",\"boot_id\":\"123456789abcdef0123456789abcdef0\"}";
  cloudServices.publishEvent(event);
  assert(mqtt.packets.back().payload==event&&!mqtt.packets.back().retained&&ws.sent.back()==event);
  const auto eventDispatches=dispatches;
  ws.inject(envelope("after-live-event"));assert(dispatches==eventDispatches+1);
  assert(lastReply()["request_id"]=="after-live-event");
  ws.sendResult=false;cloudServices.publishEvent(event);
  assert(cloudServices.lastError().find("WebSocket event write failed")!=String::npos);
  ws.inject(envelope("lost-result"));assert(dispatches==eventDispatches+2);
  assert(cloudServices.lastError().find("WebSocket command reply write failed")!=String::npos);
  ws.sendResult=true;ws.inject(envelope("lost-result"));
  assert(dispatches==eventDispatches+2&&lastReply()["code"]=="duplicate");
  // A failed command-subscription write cannot advertise an online receiver.
  cloudServices.suspend();mqtt.subscribeResult=false;
  const auto subscriptionPackets=mqtt.packets.size();
  service(true,true,260000,true,false);
  assert(ws.isConnected()); // Healthy WS must not hide the independent MQTT failure.
  assert(!mqtt.connected()&&cloudServices.lastError().find("subscription write failed")!=String::npos);
  for(size_t i=subscriptionPackets;i<mqtt.packets.size();++i)
    assert(!mqtt.packets[i].topic.endsWith("/availability")||mqtt.packets[i].payload!="online");
  mqtt.subscribeResult=true;
  mqtt.bufferResult=false;cloudServices.applyConfiguration();const auto blockedAttempts=mqtt.connectAttempts;
  assert(cloudServices.lastError().find("buffer unavailable")!=String::npos);
  service(true,true,300000,true,false);assert(mqtt.connectAttempts==blockedAttempts&&!mqtt.connected());
  puts("Cloud services: admission/6KB streaming, identity/LWT, completion-relative reconnect, cooperative nonrecursive IO and cause/duration fixtures passed (network/crypto doubles, not broker/hardware proof).");
  puts("Noinline fragment-stack boundary and byte-exact3-fragment envelope/short-tail reuse passed; compiled/physical loop-stack acceptance remains separate.");
}
