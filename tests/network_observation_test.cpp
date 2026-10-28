#include "../firmware/esp/src/NetworkObservationPolicy.h"
#include <cassert>
#include <iostream>
int main(){
  using namespace PakarNetworkObservation;
  uint32_t epoch=0;
  assert(httpDate("Wed, 28 Oct 2026 12:34:56 GMT",epoch)&&epoch==1793190896UL);
  assert(httpDate("Thu, 29 Feb 2024 00:00:00 GMT",epoch));
  for(const char *bad:{"Wed, 29 Feb 2023 00:00:00 GMT","Wed, 28 Oct 2026 24:34:56 GMT",
      "Wed, 28 Oct 2026 12:34:60 GMT","Tue, 28 Oct 2026 12:34:56 GMT",
      "Wed, 28 Oct 2026 12:34:56 UTC","Wed, 28 Oct 2026 12:34:56 GMTX",
      "Wed, 28 Oct 2026 12:34:56 GMT\r\n","Wed, 00 Oct 2026 12:34:56 GMT"})assert(!httpDate(bad,epoch));
  for(const char *good:{"1.1.1.1","8.8.8.8","93.184.215.14"})assert(publicIpv4(good));
  for(const char *bad:{"","1.1.1.1x","01.1.1.1","1.1.1.256","127.0.0.1","10.0.0.1",
      "192.168.1.1","172.16.0.1","169.254.0.1","100.64.0.1","224.1.1.1","255.255.255.255",
      "192.0.2.1","198.51.100.5","203.0.113.1","0.0.0.0"})assert(!publicIpv4(bad));
  assert(hostname("pool.ntp.org")&&hostname("ledcontroller.local"));
  for(const char *bad:{"","-host","host-","host..local","host/evil","user@host","host:80","host\r\n"})assert(!hostname(bad));
  std::cout<<"Network observation: strict UTC dates, weekday, public IPv4 and host bounds passed\n";
}
