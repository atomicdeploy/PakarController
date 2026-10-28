#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <mdns.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <atomic>

// One relay worker owns this resolver. SDK hostByName/getaddrinfo has no caller
// deadline. .local uses the actual mDNS API, not the router's unicast DNS.
// Unicast callbacks retain object-owned storage after a timeout (never a task
// stack pointer); an outstanding query is not overwritten by a later request.
class PeerLanResolver {
 public:
  bool lookup(const String &host,IPAddress &out) {
    constexpr uint32_t budget=900;
    if(host.endsWith(".local")) {
      esp_ip4_addr_t address{};
      const String label=host.substring(0,host.length()-6);
      if(mdns_query_a(label.c_str(),budget,&address)!=ESP_OK)return false;
      out=IPAddress(address.addr);return true;
    }
    if(state_.load()==Pending)return false;
    host.toCharArray(host_,sizeof(host_));
    state_=Pending;
    if(tcpip_callback(start,this)!=ERR_OK){state_=Idle;return false;}
    const uint32_t began=millis();
    while(state_.load()==Pending&&uint32_t(millis()-began)<budget)vTaskDelay(1);
    if(state_.load()!=Ready)return false;
    out=IPAddress(bytes_[0],bytes_[1],bytes_[2],bytes_[3]);return true;
  }
 private:
  enum {Idle,Pending,Ready,Failed};
  std::atomic<int> state_{Idle};char host_[254]{};uint8_t bytes_[4]{};
  static void done(const char *,const ip_addr_t *ip,void *arg) {
    auto &s=*static_cast<PeerLanResolver*>(arg);
    if(!ip||!IP_IS_V4(ip)){s.state_=Failed;return;}
    const auto *v4=ip_2_ip4(ip);
    s.bytes_[0]=ip4_addr1(v4);s.bytes_[1]=ip4_addr2(v4);
    s.bytes_[2]=ip4_addr3(v4);s.bytes_[3]=ip4_addr4(v4);s.state_=Ready;
  }
  static void start(void *arg) {
    auto &s=*static_cast<PeerLanResolver*>(arg);ip_addr_t ip{};
    const err_t result=dns_gethostbyname_addrtype(s.host_,&ip,done,&s,LWIP_DNS_ADDRTYPE_IPV4);
    if(result==ERR_OK)done(s.host_,&ip,&s);
    else if(result!=ERR_INPROGRESS)s.state_=Failed;
  }
};
#endif
