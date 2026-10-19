// Compile the actual shared handler, config stores and ArduinoJson String path.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../firmware/esp8266/src/CloudConfig.h"
#include "../firmware/esp8266/src/NetworkConfig.h"
#include <cassert>
#include <iostream>
NetworkFixtureWiFi WiFi;
struct RemoteFixture { bool active=false; bool busy()const{return active;} } remoteFirmware;
bool otaActive=false,avrUploadOwner=false,programmingLatched=false;
bool networkApplyPending=false,cloudApplyPending=false;
uint32_t otaRebootAt=0,networkApplyAt=0,cloudApplyAt=0;
struct CloudFixture {
  bool mqttConnected()const{return false;}
  int mqttState()const{return -1;}
  bool webSocketConnected()const{return false;}
  const char* lastError()const{return "fixture \"quoted\"\nerror";}
} cloudServices;
namespace NetworkPath {
bool ready(){return false;}
const char* interfaceName(){return "none";}
struct Ethernet { bool live=false;bool configured()const{return NetworkConfigStore::ethernetBuild();}bool connected()const{return live;} } port;
Ethernet& ethernet(){return port;}
}
#include "../firmware/esp8266/src/CloudManagement.h"
namespace {
JsonDocument patch(const char* text) {
  JsonDocument d;assert(!deserializeJson(d,text));return d;
}
bool cloud(const char* text,String& result,bool internet=false) {
  auto d=patch(text);return updateCloudConfiguration(d.as<JsonObjectConst>(),result,internet);
}
bool network(const char* text,String& result) {
  auto d=patch(text);return updateNetworkConfiguration(d.as<JsonObjectConst>(),result);
}
JsonDocument snapshot(const String& text) {
  JsonDocument d;assert(!deserializeJson(d,text));return d;
}
void applied(){networkApplyPending=false;cloudApplyPending=false;}
}
int main() {
  fs::FS filesystem;assert(cloudConfig.begin(filesystem));assert(networkConfig.begin(filesystem));
  pakarTestMillis=123;
  String result;
  assert(cloud("{\"mqtt_enabled\":true,\"mqtt_tls\":true,\"mqtt_host\":\"mqtt.fixture.invalid\",\"mqtt_password\":\"fixture-secret\",\"telemetry_seconds\":17}",result,true));
  auto accepted=snapshot(result);
  assert(accepted["accepted"]==true&&accepted["apply_pending"]==true&&accepted["apply_after_ms"]==1000);
  assert(cloudApplyPending&&cloudApplyAt==1123&&cloudConfig.telemetrySeconds()==17);
  const auto committed=filesystem.files.at("/cloud.bin")->bytes;
  assert(!cloud("{\"telemetry_seconds\":18}",result)); // Pending reconnect fences writes.
  applied();
  auto info=snapshot(cloudConfigurationJson());
  assert(info["mqtt_password_set"]==true&&info["mqtt_password"].isNull());
  assert(info["last_error"]=="fixture \"quoted\"\nerror"&&info["mqtt_state"]==-1);
  for(const char* invalid:{"{\"mqtt_enabled\":1}","{\"mqtt_tls\":null}","{\"mqtt_port\":0}",
      "{\"mqtt_port\":65536}","{\"telemetry_seconds\":1.5}","{\"telemetry_seconds\":-1}",
      "{\"telemetry_seconds\":3601}","{\"mqtt_host\":false}","{\"mqtt_host\":\"a\\u0000b\"}","{\"unknown\":1}"}) {
    assert(!cloud(invalid,result));assert(!cloudApplyPending);
    assert(cloudConfig.telemetrySeconds()==17&&filesystem.files.at("/cloud.bin")->bytes==committed);
  }
  assert(!cloud("{\"mqtt_tls\":false}",result,true));
  filesystem.faults->failRename=true;
  assert(!cloud("{\"telemetry_seconds\":18}",result));
  assert(!cloudApplyPending&&cloudConfig.telemetrySeconds()==17&&filesystem.files.at("/cloud.bin")->bytes==committed);
  filesystem.faults->failRename=false;
  assert(cloud("{\"telemetry_seconds\":18}",result));applied();
  assert(String(cloudConfig.mqttPassword())=="fixture-secret");
  const auto writes=filesystem.faults->writes;
  assert(cloud("{\"telemetry_seconds\":18}",result));applied();
  assert(filesystem.faults->writes==writes);
  assert(cloud("{\"mqtt_tls\":false}",result));applied(); // Authorized local plain service.

  assert(network("{\"station_enabled\":true,\"ssid\":\"fixture-ssid\",\"password\":\"fixture-password\"}",result));
  assert(networkApplyPending&&networkApplyAt==1123);applied();
  assert(network("{\"address\":\"192.0.2.7\",\"gateway\":\"192.0.2.1\",\"subnet\":\"255.255.255.0\",\"dns1\":\"192.0.2.1\"}",result));
  assert(networkConfig.dhcpEnabled()&&networkConfig.stationAddress().toString()=="192.0.2.7");applied();
  assert(network("{\"dhcp\":false}",result));applied();
  assert(!networkConfig.dhcpEnabled()&&networkConfig.stationAddress().toString()=="192.0.2.7");
  const auto networkCommitted=filesystem.files.at("/network.bin")->bytes;
  assert(!network("{\"ssid\":\"different\"}",result));
  assert(!network("{\"ap_enabled\":null}",result));
  assert(!network("{\"address\":\"bad\"}",result));
  filesystem.faults->failRename=true;
  assert(!network("{\"dhcp\":true}",result));
  assert(!networkConfig.dhcpEnabled()&&!networkApplyPending&&filesystem.files.at("/network.bin")->bytes==networkCommitted);
  filesystem.faults->failRename=false;
  auto net=snapshot(networkConfigurationJson());
  assert(net["station_ssid"]=="fixture-ssid"&&net["password"].isNull());
  assert(net["uplink"]["interface"]=="none"&&net["uplink"]["ipv4_ready"]==false);
  assert(net["ethernet"]["configured"]==NetworkConfigStore::ethernetBuild());
  assert(net["ethernet_ready"]==false);
  // Compiling Ethernet or forging a client field is never handoff evidence.
  assert(!network("{\"station_enabled\":false,\"ap_enabled\":false,\"ethernet_ready\":true}",result));
  assert(networkConfig.stationEnabled()&&networkConfig.accessPointEnabled()&&!networkApplyPending);
  NetworkPath::port.live=true;
#if defined(ARDUINO_ARCH_ESP32)
  assert(snapshot(networkConfigurationJson())["ethernet_ready"]==true);
#endif
  if(NetworkConfigStore::ethernetBuild()) {
    assert(network("{\"station_enabled\":false,\"ap_enabled\":false}",result));applied();
    assert(!networkConfig.stationEnabled()&&!networkConfig.accessPointEnabled());
    NetworkPath::port.live=false;
    assert(!network("{\"dhcp\":true}",result)); // Actual lost Ethernet, not cached readiness.
    assert(network("{\"ap_enabled\":true}",result));applied(); // Recovery remains reachable.
    assert(networkConfig.accessPointEnabled());
  } else assert(!network("{\"station_enabled\":false,\"ap_enabled\":false}",result));
  NetworkPath::port.live=false;
  for(bool* busy:{&otaActive,&avrUploadOwner,&programmingLatched,&networkApplyPending,&cloudApplyPending,&remoteFirmware.active}) {
    *busy=true;assert(!cloud("{\"telemetry_seconds\":19}",result));assert(!network("{\"dhcp\":true}",result));*busy=false;
  }
  otaRebootAt=1;assert(!cloud("{\"telemetry_seconds\":19}",result));otaRebootAt=0;
  std::cout<<"Actual cloud/network shared configuration: ACK, exact types, redaction/escaping, candidate commit, failure preservation, pending fences, DHCP/static profile passed\n";
}
