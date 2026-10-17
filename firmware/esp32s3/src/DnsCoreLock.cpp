#include <lwip/tcpip.h>

#if !LWIP_TCPIP_CORE_LOCKING
#error "S3 DNS cache adapter requires the reviewed ESP-IDF core-locking configuration"
#endif

// Arduino-ESP32 3.3.12 NetworkManager::hostByName clears the lwIP DNS cache
// outside the core lock. Clearing invokes pending DNS callbacks synchronously;
// an SNTP callback can allocate UDP state and panic. Wrap only that short call,
// never blocking DNS/TLS/socket operations. Keep all original DNS/SNI/CA checks.
// The build wrapper supplies --wrap=dns_clear_cache; shared SDK files are untouched.
extern "C" void __real_dns_clear_cache(void);
extern "C" void __wrap_dns_clear_cache(void) {
  const bool alreadyHeld=sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER);
  if(!alreadyHeld) LOCK_TCPIP_CORE();
  __real_dns_clear_cache();
  if(!alreadyHeld) UNLOCK_TCPIP_CORE();
}
