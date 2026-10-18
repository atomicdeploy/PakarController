#include <assert.h>
#include <time.h>
#include <ArduinoJson.h>
#include "CloudServices.h"

static unsigned dispatches=0,fullSnapshots=0;
static String dispatched;
static String statusJson=String("{\"data\":\"")+std::string(6000,'x')+"\"}";
static bool handler(const String &message,String &out){++dispatches;dispatched=message;out="submitted";return true;}
static String status(){return statusJson;}
static String full(){++fullSnapshots;return "{\"full\":true}";}
static const char *board="esp32s3-907069eac800",*boot="123456789abcdef0123456789abcdef0";
static String envelope(const char *id,const char *command="cell") {
  JsonDocument d;d["command"]=command;d["board_id"]=board;d["boot_id"]=boot;
  d["request_id"]=id;d["expires_at"]=static_cast<uint32_t>(time(nullptr))+60;
  String out;serializeJson(d,out);return out;
}
static JsonDocument lastReply(){JsonDocument d;assert(!deserializeJson(d,WebSocketsClient::latest->sent.back()));return d;}
int main(){
  fs::FS filesystem;assert(cloudConfig.begin(filesystem));
  assert(cloudConfig.setMqtt(true,false,"broker.local",1883,"","","pakar",true));
  assert(cloudConfig.setWebSocket(true,false,"cloud.local",80,"/ws","",true));
  assert(cloudConfig.setTelemetrySeconds(1));
  cloudServices.begin("ledboard",handler,status,full,board,boot);
  auto &mqtt=*PubSubClient::latest;auto &ws=*WebSocketsClient::latest;
  assert(mqtt.bufferSize==2048);
  cloudServices.service(true,true,1000);
  assert(mqtt.connectAttempts==1&&ws.connectAttempts==1&&fullSnapshots==1);
  assert(mqtt.clientIds.back()==board); // Human hostnames are not unique broker client IDs.
  bool large=false;
  for(const auto &p:mqtt.packets)if(p.topic.endsWith("/status")){assert(p.payload==statusJson&&p.retained);large=true;}
  assert(large&&mqtt.largestWrite<=256&&mqtt.beginCalls==mqtt.endCalls);
  JsonDocument hello;assert(!deserializeJson(hello,ws.sent.front()));
  assert(hello["board_id"]==board&&hello["boot_id"]==boot&&hello["command_fencing"]==true);
  assert(hello["command_capacity"]==BoardCore::CloudCommand::Guard::kCapacity&&hello["expires_at_unit"]=="unix_seconds");

  ws.inject("{\"command\":\"cell\"}");assert(dispatches==0);
  assert(lastReply()["code"]=="invalid_envelope");
  ws.inject("{\"command\":\"status\"}");assert(dispatches==1&&lastReply()["accepted"]==true);
  auto command=envelope("once");ws.inject(command);assert(dispatches==2);
  assert(lastReply()["request_id"]=="once");
  mqtt.inject(command);assert(dispatches==2); // One cache for both channels.
  JsonDocument duplicate;assert(!deserializeJson(duplicate,mqtt.packets.back().payload));
  assert(duplicate["accepted"]==false&&duplicate["code"]=="duplicate"&&duplicate["request_id"]=="once");
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

  const auto previousFull=fullSnapshots,previousMqtt=mqtt.connectAttempts,previousWs=ws.connectAttempts;
  cloudServices.service(true,true,2000,false,false);
  assert(fullSnapshots==previousFull&&mqtt.connectAttempts==previousMqtt&&ws.connectAttempts==previousWs);
  assert(mqtt.loops>=2&&ws.receives>0&&ws.pings>0&&ws.timeouts>0);
  ws.loseTcp();cloudServices.service(true,true,3000,false,false);
  assert(ws.connectAttempts==previousWs&&!ws.isConnected());
  cloudServices.suspend();cloudServices.service(true,true,4000,false,false);
  assert(mqtt.connectAttempts==previousMqtt&&ws.connectAttempts==previousWs);
  assert(mqtt.packets.back().topic=="pakar/ledboard/availability"&&mqtt.packets.back().payload=="offline"&&mqtt.packets.back().retained);
  cloudServices.service(true,true,12000,true,false);
  assert(mqtt.connectAttempts==previousMqtt+1&&ws.connectAttempts==previousWs+1);
  // A short stream is closed, never finished/replayed over the damaged session.
  const auto ends=mqtt.endCalls;mqtt.shortWriteLimit=20;
  cloudServices.service(true,true,13000,false,false);
  assert(mqtt.endCalls==ends&&!mqtt.connected());
  // A zero reconnect sentinel is immediate even after the signed-half-range of
  // millis uptime. Initial/reconfigured connections must not stall for weeks.
  mqtt.shortWriteLimit=SIZE_MAX;cloudServices.suspend();
  constexpr uint32_t highUptime=0x90000000U;
  cloudServices.service(true,true,highUptime,true,false);
  assert(mqtt.connected()&&mqtt.clientIds.back()==board);
  cloudServices.applyConfiguration();
  cloudServices.service(true,true,highUptime+1,true,false);
  assert(mqtt.connected());
  auto disconnects=mqtt.disconnects;
  cloudServices.renameDevice("renamed");
  assert(mqtt.disconnects==disconnects+1);
  assert(mqtt.packets.back().topic=="pakar/ledboard/availability"&&mqtt.packets.back().payload=="offline");
  cloudServices.service(true,true,highUptime+1000,true,false);assert(mqtt.connected());
  assert(cloudConfig.setMqtt(true,false,"broker.local",1883,"","","changed",false));
  cloudServices.applyConfiguration();
  assert(mqtt.packets.back().topic=="pakar/renamed/availability"&&mqtt.packets.back().payload=="offline");
  cloudServices.service(true,true,highUptime+2000,true,false);assert(mqtt.connected());
  cloudServices.service(false,true,highUptime+3000,false,false);
  assert(mqtt.packets.back().topic=="changed/renamed/availability"&&mqtt.packets.back().payload=="offline");
  cloudServices.service(true,true,highUptime+20000,true,false);assert(mqtt.connected());
  disconnects=mqtt.disconnects;mqtt.shortWriteLimit=0;
  cloudServices.suspend();
  assert(!mqtt.connected()&&mqtt.disconnects==disconnects); // Failed offline write leaves LWT possible.
  mqtt.shortWriteLimit=SIZE_MAX;
  cloudServices.begin("ledboard",handler,status,full,"invalid\nboard","invalid-boot");
  cloudServices.service(true,true,highUptime+21000,true,false);
  assert(mqtt.connected()&&mqtt.clientIds.back()!="ledboard"&&mqtt.clientIds.back().size()==32);
  puts("Cloud services: admission/6KB streaming, stable client identity, old-topic offline/LWT, high-uptime and no-reconnect fixtures passed (network/crypto doubles, not broker/hardware proof).");
}
