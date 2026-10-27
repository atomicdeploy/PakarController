#include "../firmware/esp/src/NetworkPath.h"
#include "../firmware/esp32s3/src/DiscoveryInterfaces.h"
#include <cassert>
#include <cstdio>
#include <cstring>
NetworkConfigStore networkConfig;
static void unchanged(){assert(!NetworkPath::service());}
static void discoveryPolicy(){
  using namespace PakarS3;
  DiscoveryInterfaces empty;assert(!empty.forRemote(0));
  DiscoveryInterface e;e.address=0x0a000005;e.mask=0xff000000;e.kind="ethernet";
  assert(empty.add(e));assert(!empty.add(e));
  DiscoveryInterface zero;assert(!empty.add(zero));
  DiscoveryInterface ap;ap.address=0x0a010001;ap.mask=0xffff0000;ap.kind="access-point";
  assert(empty.add(ap));
  assert(empty.forRemote(0x0a010007)->address==ap.address); // Actual /16 wins /8.
  assert(empty.forRemote(0x0a090001)->address==e.address);
  assert(empty.forRemote(0xc0000201)->address==e.address); // Stable preferred fallback.
}
int main(){
  using namespace NetworkPath;
  discoveryPolicy();
  assert(!ready());unchanged();
  WiFi.mode=WIFI_AP;WiFi.ap=IPAddress(192,168,4,1);
  assert(!ready());unchanged(); // An AP is never an uplink.
  auto interfaces=PakarS3::activeDiscoveryInterfaces();
  assert(interfaces.count==1 && strcmp(interfaces.entries[0].kind,"access-point")==0);
  assert(interfaces.entries[0].mac[5]==2);
  WiFi.connection=WL_CONNECTED;WiFi.mode=WIFI_AP_STA;
  unchanged(); // Connected flag without IP is insufficient.
  WiFi.address=IPAddress(192,168,10,20);WiFi.gateway=IPAddress(192,168,10,1);
  WiFi.dns[0]=IPAddress(192,168,10,1);
  assert(service() && ready() && strcmp(interfaceName(),"wifi")==0);
  assert(WiFi.STA.defaults==1);unchanged();
  WiFi.address=IPAddress(192,168,10,21);assert(service());unchanged();
  WiFi.gateway=IPAddress(192,168,10,2);assert(service());unchanged();
  const auto wifiRoutes=state().routeChanges,wifiDns=state().resolverChanges;
  WiFi.dns[0]=IPAddress(192,168,10,3);assert(!service());unchanged();
  assert(state().resolver==WiFi.dns[0] && state().routeChanges==wifiRoutes);
  assert(state().resolverChanges==wifiDns+1);
  assert(WiFi.STA.defaults==3);
  WiFi.STA.defaultResult=false;WiFi.address=IPAddress(192,168,10,22);
  assert(service() && !ready());assert(!service() && !ready());
  WiFi.STA.defaultResult=true;assert(service() && ready());unchanged();
  interfaces=PakarS3::activeDiscoveryInterfaces();
  assert(interfaces.count==2 && strcmp(interfaces.entries[0].kind,"station")==0);
  assert(interfaces.forRemote(PakarS3::discoveryAddress(IPAddress(192,168,4,7)))->mac[5]==2);
#if PAKAR_S3_ETHERNET_ENABLED
  begin("ledboard");assert(ETH.begins==1 && ETH.configs==1 && ETH.defaults==0);
  unchanged(); // Configured W5500 without link/IP cannot displace Wi-Fi.
  ETH.start=ETH.carrier=ETH.up=ETH.ipKnown=true;ETH.address=IPAddress(10,20,30,40);
  ETH.gateway=IPAddress(10,20,30,1);ETH.mask=IPAddress(255,255,255,0);ETH.dns[0]=IPAddress(10,20,30,2);
  assert(service() && strcmp(interfaceName(),"ethernet")==0 && ETH.defaults==1);unchanged();
  ETH.address=IPAddress(10,20,30,41);assert(service());unchanged();
  ETH.gateway=IPAddress(10,20,30,3);assert(service());unchanged();
  const auto ethernetRoutes=state().routeChanges,ethernetDns=state().resolverChanges;
  ETH.dns[0]=IPAddress(10,20,30,4);assert(!service());unchanged();
  assert(state().resolver==ETH.dns[0] && state().routeChanges==ethernetRoutes);
  assert(state().resolverChanges==ethernetDns+1);
  assert(ETH.defaults==3);
  interfaces=PakarS3::activeDiscoveryInterfaces();
  assert(interfaces.count==3 && strcmp(interfaces.entries[0].kind,"ethernet")==0);
  assert(interfaces.entries[0].mac[5]==3);
  assert(interfaces.forRemote(PakarS3::discoveryAddress(IPAddress(10,20,30,7)))->mac[5]==3);
  ETH.defaultResult=false;ETH.address=IPAddress(10,20,30,42);
  assert(service() && ready() && strcmp(interfaceName(),"wifi")==0);
  const auto fallbackRoutes=state().routeChanges,fallbackDns=state().resolverChanges;
  assert(!service() && ready() && strcmp(interfaceName(),"wifi")==0);
  assert(state().routeChanges==fallbackRoutes&&state().resolverChanges==fallbackDns);
  assert(localIP()==WiFi.localIP()&&dnsIP()==WiFi.dnsIP(0));
  ETH.defaultResult=true;assert(service() && ready());unchanged();
  ETH.dns[0]=IPAddress();ETH.dns[1]=IPAddress(10,20,30,9);
  setEthernetDhcp(false);
  serviceDns(10000,true);assert(ETH.dns[0]==IPAddress() && ETH.dnsWrites==0);
  setEthernetDhcp(true);
  serviceDns(10000,true);assert(ETH.dns[0]==IPAddress(10,20,30,9) && ETH.dnsWrites==1);
  serviceDns(10001,true);assert(ETH.dnsWrites==1);
  ETH.carrier=false;
  assert(service() && strcmp(interfaceName(),"wifi")==0);unchanged();
  interfaces=PakarS3::activeDiscoveryInterfaces();assert(interfaces.count==2);
  WiFi.connection=0;assert(service() && !ready());unchanged();
  assert(ETH.begins==1 && ETH.configs==1 && ETH.ends==0);
  assert(ethernet().end());
#else
  begin("ledboard");assert(!ethernet().configured());
  assert(strcmp(interfaceName(),"wifi")==0);unchanged();
  WiFi.connection=0;assert(service() && !ready());unchanged();
#endif
  assert(WiFi.begins==0 && WiFi.configs==0 && WiFi.disconnects==0);
  puts("NetworkPath + discovery interface policy passed with SDK doubles; no physical route, multicast or Internet proof.");
}
