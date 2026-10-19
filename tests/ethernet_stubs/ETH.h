#pragma once
#define PAKAR_ETHERNET_STUB_INCLUDED 1
#define CONFIG_ETH_ENABLED 1
#define CONFIG_ETH_SPI_ETHERNET_W5500 1
#include "Arduino.h"
#include "IPAddress.h"
#include "SPI.h"
#include "esp_eth_driver.h"
#include <functional>
using network_event_handle_t = size_t;
enum arduino_event_id_t { ARDUINO_EVENT_ETH_START };
struct arduino_event_info_t { void *eth_started = nullptr; };
struct EthernetFixtureNetwork {
  std::function<void(arduino_event_id_t, arduino_event_info_t)> callback;
  bool registrationResult = true;
  unsigned registrations = 0, removals = 0;
  network_event_handle_t onEvent(decltype(callback) value, arduino_event_id_t) {
    ++registrations;
    if (!registrationResult) return 0;
    callback = value; return 1;
  }
  void removeEvent(network_event_handle_t) { ++removals; callback = {}; }
};
inline EthernetFixtureNetwork Network;
constexpr int ETH_PHY_W5500 = 7;
struct EthernetFixtureDevice {
  bool beginResult = true, configResult = true, hostnameResult = true, endResult = true;
  bool start = false, carrier = false, up = false, ipKnown = false;
  void *driver = nullptr, *interface = nullptr;
  IPAddress address, gateway{192,168,1,1}, mask{255,255,255,0}, dns{1,1,1,1};
  String hostname;
  unsigned begins = 0, ends = 0, configs = 0, defaults = 0;
  int usedType = -1, usedPhy = -1, usedCs = -1, usedIrq = -1, usedReset = -1, usedBus = -1, usedMhz = -1;
  bool begin(int type, int phy, int cs, int irq, int rst, SPIClass &spi, uint8_t mhz) {
    ++begins; usedType=type; usedPhy=phy; usedCs=cs; usedIrq=irq; usedReset=rst; usedBus=spi.bus; usedMhz=mhz;
    driver = this; interface = this;
    esp_eth_update_input_path_info(driver,ethernetFixtureInput,interface);
    if (Network.callback) Network.callback(ARDUINO_EVENT_ETH_START, {driver});
    return beginResult;
  }
  void end() {
    ++ends;
    if (!endResult) return;
    driver = interface = nullptr; start=carrier=up=ipKnown=false;
  }
  void *handle() const { return driver; }
  void *netif() const { return interface; }
  bool setHostname(const char *value) { if (!hostnameResult) return false; hostname=value; return interface; }
  bool config(IPAddress ip=IPAddress(), IPAddress gw=IPAddress(), IPAddress sm=IPAddress(),
              IPAddress firstDns=IPAddress(), IPAddress=IPAddress()) {
    ++configs; ipKnown=false; address=ip; gateway=gw; mask=sm; dns=firstDns; return configResult;
  }
  bool started() const { return start; }
  bool connected() const { return carrier; }
  bool hasIP() const { return ipKnown; }
  bool linkUp() const { return up; }
  bool setDefault() { ++defaults; return true; }
  IPAddress localIP() const { return address; }
  IPAddress gatewayIP() const { return gateway; }
  IPAddress subnetMask() const { return mask; }
  IPAddress dnsIP(uint8_t) const { return dns; }
  String macAddress() const { return "02:00:00:00:00:01"; }
  uint16_t linkSpeed()const{return 100;}
  bool fullDuplex()const{return true;}
};
inline EthernetFixtureDevice ETH;
