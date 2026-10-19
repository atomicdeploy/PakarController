#include "EthernetPort.h"
#if PAKAR_S3_ETHERNET_ENABLED
#include "EthernetLldp.h"
#endif

namespace PakarS3 {
#if PAKAR_S3_ETHERNET_ENABLED
namespace {
bool validHostname(const char *value) {
  if (!value) return false;
  const size_t length = strlen(value);
  if (!length || length > 63 || value[0] == '-' || value[length - 1] == '-') return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}
bool nonzero(IPAddress address) {
  return address[0] || address[1] || address[2] || address[3];
}
uint32_t ipv4(IPAddress address) {
  return static_cast<uint32_t>(address[0]) << 24 |
         static_cast<uint32_t>(address[1]) << 16 |
         static_cast<uint32_t>(address[2]) << 8 | address[3];
}
bool validStatic(IPAddress address, IPAddress gateway, IPAddress subnet,
                 IPAddress dns1, IPAddress dns2) {
  // Same current native Addressing::validate host/subnet/DNS bounds. This
  // validates a separate Ethernet profile, never mutates Wi-Fi persistence.
  const uint32_t a=ipv4(address), g=ipv4(gateway), m=ipv4(subnet), h=~m;
  return h >= 3 && h != UINT32_MAX && (h & (h + 1U)) == 0 &&
      address[0] != 0 && address[0] < 224 && (a & h) != 0 && (a & h) != h &&
      gateway[0] != 0 && gateway[0] < 224 && (g & h) != 0 && (g & h) != h &&
      (a & m) == (g & m) && a != g && dns1[0] < 224 && dns2[0] < 224;
}
} // namespace
#endif

bool EthernetPort::begin(const char *hostname) {
  error_ = String();
#if PAKAR_S3_ETHERNET_ENABLED
  if (!validHostname(hostname)) { error_ = F("Invalid Ethernet hostname"); return false; }
  if (owned_) {
    if (!initialized_) { error_ = F("Ethernet driver requires explicit recovery before restart"); return false; }
    if (ETH.setHostname(hostname)) {renameEthernetLldp(this,hostname);return true;}
    error_ = F("Cannot update Ethernet hostname"); return false;
  }
  if (ETH.handle() || ETH.netif()) {
    error_ = F("Native Ethernet interface already has another owner"); return false;
  }
  if (!spi_.begin(PAKAR_S3_W5500_SCK, PAKAR_S3_W5500_MISO, PAKAR_S3_W5500_MOSI, -1)) {
    error_ = F("Cannot start dedicated W5500 SPI3 bus"); return false;
  }
  busStarted_ = true;
  owned_ = true; // Also own cleanup of a partially initialized SDK driver.
  EthernetLldpSetup observer(this);
  if (!ETH.begin(ETH_PHY_W5500, 1, PAKAR_S3_W5500_CS, PAKAR_S3_W5500_IRQ,
                 PAKAR_S3_W5500_RESET, spi_, 20)) {
    const bool released = end();
    error_ = released ? F("Native W5500 initialization failed") :
                       F("W5500 initialization failed; retained driver requires recovery");
    return false;
  }
  // The official example sets hostname in START before DHCP. Here explicit
  // config() stops/restarts DHCP AFTER hostname is set (actual SDK behavior).
  // No queued application callback can later overwrite a newer rename.
  if (!ETH.setHostname(hostname) || !ETH.config()) {
    const bool released = end();
    error_ = released ? F("Cannot configure Ethernet hostname/DHCP") :
                       F("Ethernet configuration failed; retained driver requires recovery");
    return false;
  }
  initialized_ = addressingReady_ = true;
  attachEthernetLldp(this,ETH.handle(),hostname,observer.admitted(ETH.handle()));
  return true; // Driver initialized, not link/IP/Internet acceptance.
#else
  (void)hostname;
  error_ = F("W5500 Ethernet is not compiled into this firmware");
  return false;
#endif
}

bool EthernetPort::initialized() const {
#if PAKAR_S3_ETHERNET_ENABLED
  return owned_ && initialized_;
#else
  return false;
#endif
}

bool EthernetPort::ensureInitialized(const char *hostname) {
#if PAKAR_S3_ETHERNET_ENABLED
  if (initialized()) return begin(hostname); // Rename only; never reset a healthy SPI driver.
  if (!end()) return false; // Never steal a driver whose cleanup was refused.
#endif
  return begin(hostname);
}

bool EthernetPort::configureAddressing(bool dhcp, IPAddress address,
    IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2) {
#if PAKAR_S3_ETHERNET_ENABLED
  if (!owned_ || !initialized_) { error_ = F("Ethernet driver is not initialized"); return false; }
  if (!dhcp && !validStatic(address, gateway, subnet, dns1, dns2)) {
    error_ = F("Invalid Ethernet static subnet, host, gateway or DNS"); return false;
  }
  addressingReady_ = dhcp ? ETH.config() : ETH.config(address, gateway, subnet, dns1, dns2);
  if (!addressingReady_) { error_ = F("Ethernet addressing was not accepted by the SDK"); return false; }
  error_ = String(); return true;
#else
  (void)dhcp; (void)address; (void)gateway; (void)subnet; (void)dns1; (void)dns2;
  error_ = F("W5500 Ethernet is not compiled into this firmware"); return false;
#endif
}

bool EthernetPort::end() {
#if PAKAR_S3_ETHERNET_ENABLED
  if (!owned_ && !busStarted_) return true;
  detachEthernetLldp(this);
  initialized_ = addressingReady_ = false;
  if (owned_) {
    // SDK end() owns its task/PHY/netif teardown. If it retains a handle on
    // failure, never pull SPI out from underneath a still-running driver.
    ETH.end();
    if (ETH.handle() || ETH.netif()) {
      error_ = F("Native Ethernet teardown incomplete; SPI ownership retained"); return false;
    }
  }
  owned_ = false;
  releaseEthernetLldp(this);
  if (busStarted_) spi_.end();
  busStarted_ = false;
#endif
  return true;
}

void EthernetPort::service() {
  // SDK maintains link and DHCP asynchronously. Queries below read its current
  // status, never a synthetic connected flag or a loop-driven retry schedule.
#if PAKAR_S3_ETHERNET_ENABLED
  if(owned_&&initialized_&&ETH.handle()) {
    uint8_t mac[6]{};
    if(esp_eth_ioctl(ETH.handle(),ETH_CMD_G_MAC_ADDR,mac)!=ESP_OK)return;
    serviceEthernetLldp(this,linkUp(),mac,millis());
  }
#endif
}
bool EthernetPort::setLldp(bool enabled,bool receive) {
#if PAKAR_S3_ETHERNET_ENABLED
  if(owned_&&initialized_&&configureEthernetLldp(this,enabled,receive))return true;
  error_=F("LLDP receive hook unavailable or Ethernet uninitialized");
#else
  (void)enabled;(void)receive;
  error_=F("W5500 Ethernet is not compiled into this firmware");
#endif
  return false;
}
void EthernetPort::lldpStatus(LldpStatus &status)const {
  status=LldpStatus{};
#if PAKAR_S3_ETHERNET_ENABLED
  snapshotEthernetLldp(this,status,millis());
  if(!linkUp()){status.receiveActive=false;status.neighborCount=0;}
#endif
}
bool EthernetPort::started() const {
#if PAKAR_S3_ETHERNET_ENABLED
  return owned_ && initialized_ && ETH.started();
#else
  return false;
#endif
}
bool EthernetPort::linkUp() const {
#if PAKAR_S3_ETHERNET_ENABLED
  return started() && ETH.connected() && ETH.linkUp();
#else
  return false;
#endif
}
bool EthernetPort::connected() const {
#if PAKAR_S3_ETHERNET_ENABLED
  return addressingReady_ && linkUp() && ETH.hasIP() && nonzero(ETH.localIP());
#else
  return false;
#endif
}
bool EthernetPort::makeDefault() {
#if PAKAR_S3_ETHERNET_ENABLED
  return connected() && ETH.setDefault();
#else
  return false;
#endif
}
IPAddress EthernetPort::localIP() const {
#if PAKAR_S3_ETHERNET_ENABLED
  if (connected()) return ETH.localIP();
#endif
  return IPAddress();
}
IPAddress EthernetPort::gatewayIP() const {
#if PAKAR_S3_ETHERNET_ENABLED
  if (connected()) return ETH.gatewayIP();
#endif
  return IPAddress();
}
IPAddress EthernetPort::subnetMask() const {
#if PAKAR_S3_ETHERNET_ENABLED
  if (connected()) return ETH.subnetMask();
#endif
  return IPAddress();
}
IPAddress EthernetPort::dnsIP(uint8_t index) const {
#if PAKAR_S3_ETHERNET_ENABLED
  if (connected() && index < 2) return ETH.dnsIP(index);
#else
  (void)index;
#endif
  return IPAddress();
}
String EthernetPort::macAddress() const {
#if PAKAR_S3_ETHERNET_ENABLED
  if (owned_ && ETH.netif()) return ETH.macAddress();
#endif
  return String();
}
uint16_t EthernetPort::linkSpeedMbps()const {
#if PAKAR_S3_ETHERNET_ENABLED
  if(linkUp())return ETH.linkSpeed();
#endif
  return 0;
}
bool EthernetPort::fullDuplex()const {
#if PAKAR_S3_ETHERNET_ENABLED
  return linkUp()&&ETH.fullDuplex();
#else
  return false;
#endif
}
} // namespace PakarS3
