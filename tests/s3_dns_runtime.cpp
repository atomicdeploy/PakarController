#include <lwip/tcpip.h>
extern "C" void __wrap_dns_clear_cache(void);
extern "C" void dns_clear_cache(void) {
  // The real SNTP callback reaches udp_new_ip_type and asserts this same
  // ownership condition. No real sockets, scheduler or SDK are simulated here.
  assert(sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER));
  ++DnsLockFixture::clears;
  if(DnsLockFixture::nested){DnsLockFixture::nested=false;__wrap_dns_clear_cache();}
}
