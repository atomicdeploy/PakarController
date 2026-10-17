#include <lwip/tcpip.h>
#include <iostream>
extern "C" void dns_clear_cache(void);
int main(){
  using namespace DnsLockFixture;
  reset();dns_clear_cache();
  assert(clears==1 && locks==1 && unlocks==1 && !held);
  reset();held=true;dns_clear_cache();
  assert(clears==1 && !locks && !unlocks && held);
  reset();tcpipTask=true;dns_clear_cache();
  assert(clears==1 && !locks && !unlocks && tcpipTask);
  reset();nested=true;dns_clear_cache();
  assert(clears==2 && locks==1 && unlocks==1 && !held);
  std::cout<<"S3 actual DNS linker wrapper: caller/core-owner/TCPIP/nested callback guards passed\n";
}
