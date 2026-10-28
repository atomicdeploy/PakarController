#pragma once
#include "Arduino.h"
using err_t=int;struct ip_addr_t{uint8_t bytes[4]{192,168,1,201};bool v4=true;};
constexpr int ERR_OK=0,ERR_INPROGRESS=-5,LWIP_DNS_ADDRTYPE_IPV4=0;
#define IP_IS_V4(ip) ((ip)->v4)
#define ip_2_ip4(ip) (ip)
#define ip4_addr1(ip) ((ip)->bytes[0])
#define ip4_addr2(ip) ((ip)->bytes[1])
#define ip4_addr3(ip) ((ip)->bytes[2])
#define ip4_addr4(ip) ((ip)->bytes[3])
inline bool dnsDeferred=false,dnsFails=false;
inline void (*dnsCallback)(const char*,const ip_addr_t*,void*)=nullptr;
inline void *dnsArg=nullptr;inline String dnsHost;
inline err_t dns_gethostbyname_addrtype(const char *host,ip_addr_t *out,void (*fn)(const char*,const ip_addr_t*,void*),void *arg,int){
  dnsHost=host;dnsCallback=fn;dnsArg=arg;
  if(dnsDeferred)return ERR_INPROGRESS;
  if(dnsFails)return -1;
  *out={};return ERR_OK;
}
