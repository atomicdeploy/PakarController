#pragma once
#include <stdint.h>

namespace PakarS3 {
// Pure IPv4 policy shared by SSDP/MNDP. Addresses/masks use network byte order
// numerically (a<<24|b<<16|c<<8|d), independent of IPAddress's host conversion.
struct DiscoveryInterface {
  uint32_t address = 0, mask = 0;
  uint8_t mac[6]{};
  const char *kind = "";
};
struct DiscoveryInterfaces {
  DiscoveryInterface entries[3]{};
  uint8_t count = 0;
  bool add(const DiscoveryInterface &value) {
    if (!value.address || count == 3) return false;
    for (uint8_t i=0;i<count;++i) if (entries[i].address == value.address) return false;
    entries[count++] = value; return true;
  }
  const DiscoveryInterface *forRemote(uint32_t remote) const {
    const DiscoveryInterface *best = nullptr;
    unsigned bestPrefix = 0;
    for (uint8_t i=0;i<count;++i) {
      const auto &entry = entries[i];
      if (!entry.mask || (remote & entry.mask) != (entry.address & entry.mask)) continue;
      unsigned prefix=0; uint32_t bits=entry.mask;
      while (bits & 0x80000000UL) { ++prefix; bits <<= 1; }
      if (!best || prefix > bestPrefix) { best=&entry; bestPrefix=prefix; }
    }
    // Enumeration is ETH, STA, AP. AP fallback means local discovery only,
    // never Internet reachability. Equal overlapping masks retain that order;
    // without receive-interface metadata exact ingress remains unproven.
    return best ? best : count ? &entries[0] : nullptr;
  }
};
} // namespace PakarS3
