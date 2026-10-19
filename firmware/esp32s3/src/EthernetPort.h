#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include "Lldp.h"

// Optional native ESP-IDF/lwIP interface, not a separate WIZnet socket stack.
// A disabled build includes neither ETH.h nor a driver/SPI allocation.
#ifndef PAKAR_S3_ETHERNET_ENABLED
#define PAKAR_S3_ETHERNET_ENABLED 0
#endif
#if PAKAR_S3_ETHERNET_ENABLED != 0 && PAKAR_S3_ETHERNET_ENABLED != 1
#error "PAKAR_S3_ETHERNET_ENABLED must be 0 or 1"
#endif
#if PAKAR_S3_ETHERNET_ENABLED
#if !defined(ARDUINO_ARCH_ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "The optional W5500 port requires the reviewed ESP32-S3 target"
#endif
#include <ETH.h>
#include <SPI.h>
#include "BoardPins.h"
#if !CONFIG_ETH_ENABLED || !CONFIG_ETH_SPI_ETHERNET_W5500
#error "The installed ESP32 SDK must enable native W5500 Ethernet"
#endif
#endif

namespace PakarS3 {
class EthernetPort {
 public:
  EthernetPort() = default; // No pin, SPI or network operation at construction.
  EthernetPort(const EthernetPort &) = delete;
  EthernetPort &operator=(const EthernetPort &) = delete;
  static constexpr bool configured() { return PAKAR_S3_ETHERNET_ENABLED != 0; }
  // One explicit DHCP start. No Wi-Fi shutdown, fallback, automatic retry or
  // default-route selection: the shared NetworkPath owns route policy.
  bool begin(const char *hostname);
  // Explicit configuration/recovery only, never called by service().
  bool ensureInitialized(const char *hostname);
  bool initialized() const;
  bool configureAddressing(bool dhcp, IPAddress address = IPAddress(),
      IPAddress gateway = IPAddress(), IPAddress subnet = IPAddress(),
      IPAddress dns1 = IPAddress(), IPAddress dns2 = IPAddress());
  bool end();
  bool setLldp(bool enabled,bool receive=false);
  void lldpStatus(LldpStatus &status)const;
  void service();
  bool started() const;
  bool linkUp() const;
  // IPv4-ready only; this is not a claim of Internet/cloud reachability.
  bool connected() const;
  bool makeDefault();
  IPAddress localIP() const;
  IPAddress gatewayIP() const;
  IPAddress subnetMask() const;
  IPAddress dnsIP(uint8_t index = 0) const;
  String macAddress() const;
  uint16_t linkSpeedMbps()const;
  bool fullDuplex()const;
  const String &lastError() const { return error_; }
 private:
  String error_;
#if PAKAR_S3_ETHERNET_ENABLED
  // AvrIsp owns FSPI/SPI2. HSPI is SPI3 on S3, never Arduino global SPI.
  SPIClass spi_{HSPI};
  bool busStarted_ = false;
  bool owned_ = false;
  bool initialized_ = false;
  bool addressingReady_ = false;
#endif
};
} // namespace PakarS3
