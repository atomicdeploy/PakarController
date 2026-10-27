#pragma once
#include "NetworkConfig.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <firmware/esp32s3/src/EthernetPort.h>
#include <firmware/esp32s3/src/DnsRecovery.h>
#endif

// One native socket stack for HTTP, WebSocket, MQTT, NTP and OTA. AP is a
// recovery/local interface, never evidence of an Internet uplink. An assigned
// uplink address is likewise NOT evidence that the broker/Internet is reachable.
namespace NetworkPath {
enum class Interface : uint8_t { None, Wifi, Ethernet };
struct State {
  Interface selected = Interface::None;
  IPAddress address, gateway, resolver;
  uint32_t routeChanges=0,resolverChanges=0;
  bool ethernetDhcp = true;
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::EthernetPort ethernet;
#endif
};
inline State &state() { static State value; return value; }
#if defined(ARDUINO_ARCH_ESP32)
inline PakarS3::EthernetPort &ethernet() { return state().ethernet; }
#endif
inline void begin(const char *hostname,bool ethernetEnabled=true) {
#if defined(ARDUINO_ARCH_ESP32)
  if (ethernet().configured()&&ethernetEnabled) ethernet().begin(hostname);
  else if(ethernet().configured())ethernet().end();
#else
  (void)hostname;(void)ethernetEnabled;
#endif
}
inline bool wifiReady() {
  return networkConfig.stationConnected() && static_cast<uint32_t>(WiFi.localIP()) != 0;
}
inline bool ready() { return state().selected != Interface::None; }
inline const char *interfaceName() {
  return state().selected == Interface::Ethernet ? "ethernet" :
      state().selected == Interface::Wifi ? "wifi" : "none";
}
inline IPAddress localIP() { return state().address; }
inline IPAddress gatewayIP() { return state().gateway; }
inline void setEthernetDhcp(bool enabled) { state().ethernetDhcp = enabled; }
inline IPAddress dnsIP(uint8_t index = 0) {
#if defined(ARDUINO_ARCH_ESP32)
  if (state().selected == Interface::Ethernet) return ethernet().dnsIP(index);
#endif
  return state().selected == Interface::Wifi ? WiFi.dnsIP(index) : IPAddress();
}
// Returns a route-generation change, including ready->ready interface/address
// changes. The loop owner then closes old cloud sessions exactly once; commands
// are never replayed on the new route. Resolver-only changes affect FUTURE DNS
// lookups, not established TCP sessions: do not tear down working MQTT/WS for
// DHCP/RDNSS DNS refreshes. No setting/flash write occurs here.
inline bool service() {
  auto &s = state();
  Interface next = wifiReady() ? Interface::Wifi : Interface::None;
  IPAddress address = next == Interface::Wifi ? WiFi.localIP() : IPAddress();
  IPAddress gateway = next == Interface::Wifi ? WiFi.gatewayIP() : IPAddress();
  IPAddress resolver = next == Interface::Wifi ? WiFi.dnsIP(0) : IPAddress();
#if defined(ARDUINO_ARCH_ESP32)
  ethernet().service();
  if (ethernet().connected()) {
    next = Interface::Ethernet; address = ethernet().localIP();
    gateway = ethernet().gatewayIP(); resolver = ethernet().dnsIP(0);
  }
#endif
  const auto observeResolver=[&](IPAddress value) {
    if(value!=s.resolver && s.resolverChanges!=UINT32_MAX)++s.resolverChanges;
    s.resolver=value;
  };
  const bool changed = next != s.selected || address != s.address ||
      gateway != s.gateway;
  if (!changed) {observeResolver(resolver);return false;}
#if defined(ARDUINO_ARCH_ESP32)
  bool routeAccepted = next == Interface::Ethernet ? ethernet().makeDefault() :
      next == Interface::Wifi ? WiFi.STA.setDefault() : true;
  if (!routeAccepted && next==Interface::Ethernet && wifiReady() && WiFi.STA.setDefault()) {
    // A ready Ethernet netif that refuses default-route ownership must not
    // strand a separately acknowledged Wi-Fi route. No interface is reconfigured
    // and no cloud action is replayed; report only the route actually accepted.
    next=Interface::Wifi;address=WiFi.localIP();gateway=WiFi.gatewayIP();resolver=WiFi.dnsIP(0);
    routeAccepted=true;
  }
  if (!routeAccepted) {
    // Never retain a stale ready route when the replacement was refused.
    const bool lost = s.selected != Interface::None;
    s.selected = Interface::None; s.address = IPAddress();
    s.gateway = IPAddress();observeResolver(IPAddress());
    if(lost && s.routeChanges!=UINT32_MAX)++s.routeChanges;
    return lost;
  }
#endif
  const bool acceptedChange=next!=s.selected || address!=s.address || gateway!=s.gateway;
  s.selected = next; s.address = address; s.gateway = gateway;observeResolver(resolver);
  // Rejected preferred-route retries can retain this same working fallback;
  // do not repeatedly suspend its cloud sessions or count fictional changes.
  if(!acceptedChange)return false;
  if(s.routeChanges!=UINT32_MAX)++s.routeChanges;
  return true;
}
inline void serviceDns(uint32_t now, bool immediate = false) {
#if defined(ARDUINO_ARCH_ESP32)
  if (state().selected == Interface::Wifi)
    PakarS3::serviceDhcpDns(now, networkConfig.dhcpEnabled(), immediate);
#if PAKAR_S3_ETHERNET_ENABLED && PAKAR_S3_DNS_GATEWAY_FALLBACK
  // DHCP W5500 profile: same conservative backup/gateway recovery as STA.
  // Never force a public resolver or rewrite a static profile.
  if (state().selected == Interface::Ethernet && state().ethernetDhcp) {
    static uint32_t checked = 0;
    if (!immediate && static_cast<uint32_t>(now - checked) < 5000U) return;
    checked = now;
    const auto usable = [](IPAddress ip) {
      return ip.type() == IPv4 && ip[0] > 0 && ip[0] < 224 && ip[0] != 127 &&
          static_cast<uint32_t>(ip) != UINT32_MAX;
    };
    if (!usable(ETH.dnsIP(0))) {
      const IPAddress backup = ETH.dnsIP(1);
      const IPAddress candidate = usable(backup) ? backup : ETH.gatewayIP();
      if (usable(candidate)) ETH.dnsIP(0, candidate);
    }
  }
#endif
#else
  (void)now; (void)immediate;
#endif
}
} // namespace NetworkPath
