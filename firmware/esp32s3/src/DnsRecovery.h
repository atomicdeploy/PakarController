#pragma once
#include <WiFi.h>
#include <stdint.h>

#ifndef PAKAR_S3_DNS_GATEWAY_FALLBACK
#define PAKAR_S3_DNS_GATEWAY_FALLBACK 1
#endif
namespace PakarS3 {
// A DHCP/RDNSS resolver can be IPv6-only while STA has no global IPv6 route.
// Preserve working DNS/static profiles; promote a usable backup, or use only
// this authorized network's IPv4 gateway. No public resolver, static IP, flash
// write, DNS probe, or change to AP settings. SDK setter keeps DHCP running.
class DnsRecovery {
 public:
  bool service(uint32_t now,bool dhcp,bool immediate=false) {
#if PAKAR_S3_DNS_GATEWAY_FALLBACK
    if(!immediate && static_cast<uint32_t>(now-lastCheck_)<5000U) return false;
    lastCheck_=now;
    if(!dhcp || WiFi.status()!=WL_CONNECTED) return false;
    const bool v6=WiFi.STA.hasGlobalIPv6();
    if(usable(WiFi.dnsIP(0),v6)) return false;
    const IPAddress backup=WiFi.dnsIP(1);
    if(usable(backup,v6) && backup.type()==IPv6) return false;
    const IPAddress candidate=usable(backup,v6) && backup.type()==IPv4?backup:WiFi.gatewayIP();
    if(!usable(candidate,false) || candidate.type()!=IPv4) return false;
    return WiFi.STA.dnsIP(0,candidate);
#else
    (void)now;(void)dhcp;(void)immediate;return false;
#endif
  }
 private:
  static bool usable(const IPAddress &ip,bool globalV6) {
    if(ip.type()==IPv6) return globalV6;
    return ip[0]>0 && ip[0]<224 && ip[0]!=127 && static_cast<uint32_t>(ip)!=UINT32_MAX;
  }
  uint32_t lastCheck_=0;
};
inline void serviceDhcpDns(uint32_t now,bool dhcp,bool immediate=false){static DnsRecovery recovery;recovery.service(now,dhcp,immediate);}
}
