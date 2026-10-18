#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../firmware/esp8266/src/CloudConfig.h"
#include "../firmware/esp8266/src/NetworkConfig.h"
#include "../firmware/esp8266/src/EthernetConfig.h"
#include <cassert>
#include <iostream>
NetworkFixtureWiFi WiFi;
EthernetConfigStore ethernetConfig;
struct RemoteFixture {bool active=false;bool busy()const{return active;}} remoteFirmware;
bool otaActive=false,avrUploadOwner=false,programmingLatched=false;
bool networkApplyPending=false,cloudApplyPending=false,ethernetApplyOnly=false;
uint32_t otaRebootAt=0,networkApplyAt=0,cloudApplyAt=0;
struct CloudFixture {
  bool mqttConnected()const{return false;}
  int mqttState()const{return -1;}
  bool webSocketConnected()const{return false;}
  const char* lastError()const{return "";}
} cloudServices;
namespace NetworkPath {
bool ready(){return false;}
const char* interfaceName(){return "none";}
struct Ethernet {
  bool up=false;
  bool configured()const{return PAKAR_S3_ETHERNET_ENABLED!=0;}
  bool initialized()const{return true;}
  bool started()const{return true;}
  bool linkUp()const{return up;}
  bool connected()const{return up;}
  IPAddress localIP()const{return IPAddress(198,51,100,7);}
  IPAddress gatewayIP()const{return IPAddress(198,51,100,1);}
  IPAddress subnetMask()const{return IPAddress(255,255,255,0);}
  IPAddress dnsIP(uint8_t index)const{return index?IPAddress():gatewayIP();}
  String macAddress()const{return "02:00:00:00:00:01";}
  const char* lastError()const{return "fixture apply \"not connected\"";}
} port;
Ethernet& ethernet(){return port;}
}
// Both actual management headers are compiled together, not a reimplemented
// validation helper. Only driver/runtime observations are fixture substitutes.
#include "../firmware/esp8266/src/CloudManagement.h"
#include "../firmware/esp8266/src/EthernetManagement.h"
namespace {
bool update(const char* text,String& result) {
  JsonDocument d;assert(!deserializeJson(d,text));
  return updateEthernetConfiguration(d.as<JsonObjectConst>(),result);
}
JsonDocument snapshot(const String &text) {
  JsonDocument d;assert(!deserializeJson(d,text));return d;
}
void applyReceiptConsumed(){networkApplyPending=false;ethernetApplyOnly=false;}
}
int main() {
  fs::FS filesystem;assert(cloudConfig.begin(filesystem));assert(networkConfig.begin(filesystem));
  assert(ethernetConfig.begin(filesystem));
  assert(networkConfig.setStation("fixture-wifi","fixture-password",true));assert(networkConfig.save());
  const auto wifi=filesystem.files.at("/network.bin")->bytes;
  pakarTestMillis=222;
  String result;
  auto initial=snapshot(ethernetConfigurationJson());
#if PAKAR_S3_ETHERNET_ENABLED
  auto offline=snapshot(ethernetStatusJson());
  assert(offline["configured"]==true && offline["started"]==true && offline["ipv4_ready"]==false);
  assert(offline["ip"].isNull() && offline["dns1"].isNull());
  NetworkPath::port.up=true;
  auto online=snapshot(ethernetStatusJson());
  assert(online["link_up"]==true && online["ipv4_ready"]==true && online["ip"]=="198.51.100.7");
  assert(online["gateway"]=="198.51.100.1" && online["mac"]=="02:00:00:00:00:01");
  NetworkPath::port.up=false;
  assert(snapshot(ethernetStatusJson())["ip"].isNull()); // Never stale old IP.
  assert(initial["configured"]==true&&initial["dhcp"]==true&&initial["ipv4_ready"]==false);
  assert(initial["apply_error"]=="fixture apply \"not connected\"");
  assert(update("{\"address\":\"198.51.100.7\",\"gateway\":\"198.51.100.1\",\"subnet\":\"255.255.255.0\",\"dns1\":\"198.51.100.1\"}",result));
  auto receipt=snapshot(result);
  assert(receipt["accepted"]==true&&receipt["apply_pending"]==true&&receipt["apply_after_ms"]==1000);
  assert(networkApplyPending&&ethernetApplyOnly&&networkApplyAt==1222);
  assert(ethernetConfig.dhcpEnabled()&&ethernetConfig.address().toString()=="198.51.100.7");
  assert(!update("{\"dhcp\":false}",result));
  assert(snapshot(ethernetConfigurationJson())["apply_pending"]==true);
  applyReceiptConsumed();
  assert(update("{\"dhcp\":false}",result));applyReceiptConsumed();
  assert(!ethernetConfig.dhcpEnabled()&&ethernetConfig.address().toString()=="198.51.100.7");
  const auto committed=filesystem.files.at("/ethernet.bin")->bytes;
  for(const char* invalid:{"{\"dhcp\":1}","{\"dhcp\":null}","{\"address\":false}",
      "{\"address\":\"198.51.100.7\\u0000x\"}","{\"address\":\"198.51.100.0\"}","{\"ssid\":\"wrong-surface\"}"}) {
    assert(!update(invalid,result));assert(!networkApplyPending&&!ethernetApplyOnly);
    assert(filesystem.files.at("/ethernet.bin")->bytes==committed);
  }
  filesystem.faults->failRename=true;
  assert(!update("{\"dhcp\":true}",result));
  assert(!ethernetConfig.dhcpEnabled()&&!networkApplyPending&&!ethernetApplyOnly);
  assert(filesystem.files.at("/ethernet.bin")->bytes==committed);
  filesystem.faults->failRename=false;
  const auto writes=filesystem.faults->writes;
  assert(update("{\"dhcp\":false}",result));applyReceiptConsumed();
  assert(filesystem.faults->writes==writes);
  assert(update("{\"dhcp\":true}",result));applyReceiptConsumed();
  assert(ethernetConfig.dhcpEnabled()&&ethernetConfig.address().toString()=="198.51.100.7");
  for(bool* busy:{&otaActive,&avrUploadOwner,&programmingLatched,&networkApplyPending,&cloudApplyPending,&remoteFirmware.active}) {
    *busy=true;assert(!update("{\"dhcp\":false}",result));*busy=false;
  }
  otaRebootAt=1;assert(!update("{\"dhcp\":false}",result));otaRebootAt=0;
  assert(!ethernetApplyOnly);
  auto status=snapshot(ethernetConfigurationJson());
  assert(status["dhcp"]==true&&status["address"]=="198.51.100.7"&&status["ipv4_ready"]==false);
#else
  assert(snapshot(ethernetStatusJson())["configured"]==false);
  assert(initial["configured"]==false&&initial.size()==1);
  assert(!update("{\"dhcp\":false}",result));
  assert(result=="Ethernet is not compiled into this firmware");
  assert(!networkApplyPending&&!ethernetApplyOnly&&filesystem.files.count("/ethernet.bin")==0);
  // Disabled-feature validation cannot queue a future hardware action.
  applyReceiptConsumed();
#endif
  assert(filesystem.files.at("/network.bin")->bytes==wifi);
  assert(networkConfig.stationEnabled()&&String(networkConfig.stationSsid())=="fixture-wifi");
  assert(WiFi.modeValue==0&&WiFi.ssidValue.empty()); // Handler did not restart WiFi.
  std::cout<<"Actual Ethernet management feature="<<PAKAR_S3_ETHERNET_ENABLED
      <<": typed config/ACK, delayed independent apply, committed profile, DHCP preservation, no WiFi mutation passed\n";
}
