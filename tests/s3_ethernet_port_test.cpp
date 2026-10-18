#include "../firmware/esp32s3/src/EthernetPort.h"
#include <cassert>
#include <cstdio>
#if PAKAR_S3_ETHERNET_ENABLED
void resetFixture() {
  ETH = EthernetFixtureDevice{}; Network = EthernetFixtureNetwork{};
  SPIClass::starts=SPIClass::stops=0; SPIClass::beginResult=true;
}
#else
#ifdef PAKAR_ETHERNET_STUB_INCLUDED
#error "Disabled firmware must not include or instantiate the native ETH driver"
#endif
#endif
int main() {
  using PakarS3::EthernetPort;
#if PAKAR_S3_ETHERNET_ENABLED
  resetFixture();
  {
    EthernetPort port;
    assert(port.configured() && !port.started() && !port.linkUp() && !port.connected());
    assert(SPIClass::starts==0 && ETH.begins==0);
    assert(!port.begin("bad.local") && SPIClass::starts==0);
    assert(port.begin("ledboard") && ETH.hostname=="ledboard" && port.initialized());
    assert(port.ensureInitialized("ledboard") && ETH.begins==1 && SPIClass::starts==1);
    assert(SPIClass::usedBus==HSPI && SPIClass::usedBus!=FSPI);
    assert(SPIClass::usedSck==5 && SPIClass::usedMiso==6 && SPIClass::usedMosi==7 && SPIClass::usedSs==-1);
    assert(ETH.usedType==ETH_PHY_W5500 && ETH.usedPhy==1 && ETH.usedCs==8 && ETH.usedReset==9 && ETH.usedIrq==14);
    assert(ETH.usedMhz==20 && ETH.configs==1 && ETH.defaults==0);
    assert(!port.started() && !port.connected() && port.localIP().toString()=="0.0.0.0");
    ETH.start=true; ETH.carrier=true; ETH.up=true;
    assert(port.linkUp() && !port.connected() && !port.makeDefault());
    ETH.ipKnown=true; assert(!port.connected()); // A flag with no address is not readiness.
    ETH.address=IPAddress(192,168,1,20);
    ETH.gateway=IPAddress(192,168,1,1); ETH.dns=IPAddress(1,1,1,1);
    assert(port.connected() && port.localIP().toString()=="192.168.1.20");
    assert(port.makeDefault() && ETH.defaults==1);
    assert(port.gatewayIP().toString()=="192.168.1.1" && port.dnsIP().toString()=="1.1.1.1");
    assert(port.dnsIP(2).toString()=="0.0.0.0" && !port.macAddress().isEmpty());
    assert(port.begin("renamed-board") && ETH.begins==1 && ETH.hostname=="renamed-board");
    const auto beforeInvalid=ETH.configs;
    assert(!port.configureAddressing(false, IPAddress(192,168,1,0), IPAddress(192,168,1,1), IPAddress(255,255,255,0)));
    assert(!port.configureAddressing(false, IPAddress(192,168,1,20), IPAddress(192,168,2,1), IPAddress(255,255,255,0)));
    assert(!port.configureAddressing(false, IPAddress(192,168,1,20), IPAddress(192,168,1,1), IPAddress(255,0,255,0)));
    assert(!port.configureAddressing(false, IPAddress(192,168,1,20), IPAddress(192,168,1,1), IPAddress(255,255,255,0), IPAddress(224,0,0,1)));
    assert(ETH.configs==beforeInvalid);
    assert(port.configureAddressing(false, IPAddress(192,168,1,21), IPAddress(192,168,1,1), IPAddress(255,255,255,0), IPAddress(9,9,9,9)));
    assert(!port.connected()); ETH.ipKnown=true;
    assert(port.connected() && port.localIP().toString()=="192.168.1.21");
    ETH.configResult=false; assert(!port.configureAddressing(true) && !port.connected());
    ETH.configResult=true; assert(port.configureAddressing(true));
    ETH.carrier=false; port.service();
    assert(!port.connected() && port.localIP().toString()=="0.0.0.0");
    assert(port.end() && SPIClass::stops==1 && Network.removals==0);
    assert(port.end() && SPIClass::stops==1); // No double-release.
  }
  resetFixture();
  { EthernetPort port; ETH.driver=&ETH; assert(!port.begin("ledboard") && !ETH.begins && !SPIClass::starts); }
  resetFixture();
  { EthernetPort port; SPIClass::beginResult=false; assert(!port.begin("ledboard") && !ETH.begins && !SPIClass::stops); }
  resetFixture();
  { EthernetPort port; ETH.beginResult=false; assert(!port.begin("ledboard") && ETH.ends==1 && SPIClass::stops==1); }
  resetFixture();
  { EthernetPort port; ETH.beginResult=false;
    assert(!port.ensureInitialized("ledboard") && !port.initialized());
    const auto attempts=ETH.begins; port.service(); port.service();
    assert(ETH.begins==attempts); // No background retry/reset storm.
    ETH.beginResult=true;
    assert(port.ensureInitialized("ledboard") && port.initialized() && ETH.begins==attempts+1);
    assert(port.ensureInitialized("ledboard") && ETH.begins==attempts+1);
    assert(port.end() && !port.initialized()); }
  resetFixture();
  { EthernetPort port; ETH.configResult=false; assert(!port.begin("ledboard") && ETH.ends==1 && SPIClass::stops==1); }
  resetFixture();
  { EthernetPort port; assert(port.begin("ledboard")); ETH.endResult=false;
    assert(!port.end() && SPIClass::stops==0 && Network.removals==0);
    assert(!port.started() && !port.connected() && !port.begin("ledboard"));
    assert(!port.ensureInitialized("ledboard") && SPIClass::stops==0);
    ETH.endResult=true; assert(port.end() && SPIClass::stops==1); }
  puts("Native W5500 adapter lifecycle/pins/readiness assertions passed using SDK doubles, not actual Ethernet hardware or lwIP.");
#else
  EthernetPort port;
  assert(!port.configured() && !port.begin("ledboard"));
  assert(!port.initialized() && !port.ensureInitialized("ledboard"));
  assert(!port.started() && !port.linkUp() && !port.connected() && !port.makeDefault());
  assert(!port.configureAddressing(true));
  assert(port.localIP().toString()=="0.0.0.0" && port.macAddress().isEmpty() && port.end());
  puts("Disabled W5500 adapter has no ETH dependency and never reports readiness.");
#endif
}
