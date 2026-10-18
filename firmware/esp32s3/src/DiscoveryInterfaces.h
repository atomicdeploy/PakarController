#pragma once
#include <WiFi.h>
#include "DiscoveryInterfacePolicy.h"
#if defined(PAKAR_S3_ETHERNET_ENABLED) && PAKAR_S3_ETHERNET_ENABLED
#include <ETH.h>
#endif

namespace PakarS3 {
inline uint32_t discoveryAddress(IPAddress ip) {
  return static_cast<uint32_t>(ip[0]) << 24 | static_cast<uint32_t>(ip[1]) << 16 |
         static_cast<uint32_t>(ip[2]) << 8 | ip[3];
}
inline IPAddress discoveryAddress(uint32_t ip) {
  return IPAddress(static_cast<uint8_t>(ip>>24), static_cast<uint8_t>(ip>>16),
                   static_cast<uint8_t>(ip>>8), static_cast<uint8_t>(ip));
}
inline DiscoveryInterfaces activeDiscoveryInterfaces() {
  DiscoveryInterfaces result;
#if defined(PAKAR_S3_ETHERNET_ENABLED) && PAKAR_S3_ETHERNET_ENABLED
  if (ETH.started() && ETH.connected() && ETH.linkUp() && ETH.hasIP()) {
    DiscoveryInterface entry;
    entry.address=discoveryAddress(ETH.localIP()); entry.mask=discoveryAddress(ETH.subnetMask());
    ETH.macAddress(entry.mac); entry.kind="ethernet"; result.add(entry);
  }
#endif
  if (WiFi.status() == WL_CONNECTED) {
    DiscoveryInterface entry;
    entry.address=discoveryAddress(WiFi.localIP()); entry.mask=discoveryAddress(WiFi.subnetMask());
    WiFi.macAddress(entry.mac); entry.kind="station"; result.add(entry);
  }
  const auto mode=WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) {
    DiscoveryInterface entry;
    entry.address=discoveryAddress(WiFi.softAPIP()); entry.mask=discoveryAddress(WiFi.softAPSubnetMask());
    WiFi.softAPmacAddress(entry.mac); entry.kind="access-point"; result.add(entry);
  }
  return result;
}
} // namespace PakarS3
