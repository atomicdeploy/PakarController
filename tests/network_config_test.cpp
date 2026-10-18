#include <cassert>
#include <cstddef>
#include "../firmware/esp8266/src/NetworkConfig.h"
NetworkFixtureWiFi WiFi;

namespace {
uint32_t digest(const uint8_t *bytes,size_t count) {
  uint32_t value=2166136261U;
  for(size_t i=0;i<count;++i)value=(value^bytes[i])*16777619U;
  return value;
}
template<typename T> void store(fs::FS &filesystem,T &record) {
  record.checksum=digest(reinterpret_cast<const uint8_t *>(&record),offsetof(T,checksum));
  File file=filesystem.open("/network.bin","w");
  assert(file.write(reinterpret_cast<const uint8_t *>(&record),sizeof(record))==sizeof(record));
}
struct Legacy {
  uint32_t magic;uint8_t schema,station;char ssid[33],password[64];uint32_t checksum;
} __attribute__((packed));
struct Previous {
  uint32_t magic;uint8_t schema,station,ap;char ssid[33],password[64];uint32_t checksum;
} __attribute__((packed));
}
int main() {
  fs::FS filesystem;NetworkConfigStore settings;
  assert(settings.begin(filesystem));
  assert(settings.accessPointEnabled()&&!settings.stationEnabled()&&settings.dhcpEnabled());
  assert(!settings.setAccessPointEnabled(false));
  assert(settings.setStation("Fixture WiFi","fixture-passphrase",true));
  assert(settings.setAccessPointEnabled(false));
  assert(settings.save());
  const auto active=filesystem.files.at("/network.bin");
  assert(settings.save());
  assert(filesystem.files.at("/network.bin")==active); // Identical save never replaces the record.
  assert(!settings.setStation(String("bad\0ssid",8),"fixture-passphrase",true));
  assert(!settings.setAddressing(false,"192.168.10.7","192.168.11.1","255.255.255.0"));
  assert(!settings.setAddressing(false,"192.168.10.0","192.168.10.1","255.255.255.0"));
  assert(!settings.setAddressing(false,"192.168.10.255","192.168.10.1","255.255.255.0"));
  assert(!settings.setAddressing(false,"192.168.10.7","192.168.10.1","255.0.255.0"));
  assert(!settings.setAddressing(false,"224.1.2.3","224.1.2.1","255.255.255.0"));
  assert(settings.setAddressing(false,"192.168.10.7","192.168.10.1","255.255.255.0","192.168.10.1","1.1.1.1"));
  assert(!settings.dhcpEnabled()&&settings.stationAddress().toString()=="192.168.10.7");
  filesystem.failWrite=true;assert(!settings.save());
  assert(filesystem.files.at("/network.bin")==active);
  filesystem.failWrite=false;filesystem.failRename=true;assert(!settings.save());
  assert(filesystem.files.at("/network.bin")==active);
  filesystem.failRename=false;assert(settings.save());
  NetworkConfigStore loaded;assert(loaded.begin(filesystem));
  assert(!loaded.dhcpEnabled()&&loaded.stationAddress().toString()=="192.168.10.7");
  assert(loaded.setAddressing(true));
  assert(loaded.stationAddress().toString()=="192.168.10.7");
  assert(loaded.setAddressing(false,loaded.stationAddress().toString(),loaded.stationGateway().toString(),
      loaded.stationSubnet().toString(),loaded.stationDns1().toString(),loaded.stationDns2().toString()));
  assert(!loaded.dhcpEnabled());
  assert(!loaded.configure("ledboard","fixture-ap-passphrase")); // AP flag is false in STA-only mode.
  assert(WiFi.modeValue==WIFI_STA&&WiFi.hostnameValue=="ledboard"&&!WiFi.persistentValue);
  assert(WiFi.address.toString()=="192.168.10.7"&&WiFi.autoReconnect);
  assert(loaded.setStation(loaded.stationSsid(),"",true,false));
  loaded.configure("ledboard","fixture-ap-passphrase");assert(WiFi.passwordValue=="fixture-passphrase");
  assert(!loaded.setStation("different-ssid","",true,false));
  assert(loaded.setStation(loaded.stationSsid(),"",true,true));
  loaded.configure("ledboard","fixture-ap-passphrase");assert(WiFi.passwordValue.empty());
  NetworkConfigStore candidate=loaded;
  assert(candidate.setAddressing(true));filesystem.failRename=true;
  assert(!candidate.save());assert(!loaded.dhcpEnabled()); // Root can publish only a committed candidate.
  filesystem.failRename=false;
  assert(loaded.setAccessPointEnabled(true));
  assert(loaded.configure("ledboard","fixture-ap-passphrase"));assert(WiFi.modeValue==WIFI_AP_STA);
#if defined(ESP32)
  assert(WiFi.nameAtStart=="ledboard" && WiFi.STA.name=="ledboard" && WiFi.AP.name=="ledboard");
  const char *longName="a-long-configured-hostname-with-no-random-suffix";
  assert(loaded.configure(longName,"fixture-ap-passphrase"));
  assert(WiFi.nameAtStart==longName && WiFi.STA.name==longName && WiFi.AP.name==longName);
  WiFi.STA.fail=true;assert(!loaded.configure("renamed-board","fixture-ap-passphrase"));
  assert(WiFi.modeValue==WIFI_OFF);WiFi.STA.fail=false;
  assert(loaded.configure("renamed-board","fixture-ap-passphrase"));
  assert(WiFi.STA.name=="renamed-board" && WiFi.AP.name=="renamed-board");
#endif
  assert(loaded.setStation("","",false));
  assert(loaded.configure("ledboard","fixture-ap-passphrase"));assert(WiFi.modeValue==WIFI_AP);
  assert(loaded.setAddressing(true));assert(loaded.save());
  fs::FS legacyFs;Legacy legacy{0x504b4e54U,1,1,"legacy-wifi","fixture-passphrase",0};store(legacyFs,legacy);
  NetworkConfigStore migrated;assert(migrated.begin(legacyFs));
  assert(migrated.stationEnabled()&&migrated.accessPointEnabled()&&migrated.dhcpEnabled());
  assert(String(migrated.stationSsid())=="legacy-wifi");
  fs::FS previousFs;Previous previous{0x504b4e54U,2,1,0,"previous-wifi","fixture-passphrase",0};store(previousFs,previous);
  NetworkConfigStore previousLoaded;assert(previousLoaded.begin(previousFs));
  assert(previousLoaded.stationEnabled()&&!previousLoaded.accessPointEnabled()&&previousLoaded.dhcpEnabled());
  assert(String(previousLoaded.stationSsid())=="previous-wifi");
  previousFs.files.at("/network.bin")->bytes.push_back(0); // Reject truncated/extra/private-layout bytes.
  NetworkConfigStore damaged;assert(!damaged.begin(previousFs));
  assert(damaged.accessPointEnabled()&&!damaged.stationEnabled());
}
