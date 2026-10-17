#pragma once
#include <cassert>
#define LWIP_TCPIP_CORE_LOCKING 1
enum sys_thread_core_lock_t {LWIP_CORE_LOCK_QUERY_HOLDER};
namespace DnsLockFixture {
inline bool held=false,tcpipTask=false,nested=false;
inline unsigned locks=0,unlocks=0,clears=0;
inline void reset(){held=tcpipTask=nested=false;locks=unlocks=clears=0;}
}
inline bool sys_thread_tcpip(sys_thread_core_lock_t){return DnsLockFixture::held || DnsLockFixture::tcpipTask;}
inline void fixtureLock(){assert(!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER));DnsLockFixture::held=true;++DnsLockFixture::locks;}
inline void fixtureUnlock(){assert(DnsLockFixture::held);DnsLockFixture::held=false;++DnsLockFixture::unlocks;}
#define LOCK_TCPIP_CORE() fixtureLock()
#define UNLOCK_TCPIP_CORE() fixtureUnlock()
