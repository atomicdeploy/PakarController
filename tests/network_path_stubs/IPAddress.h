#pragma once
#include "Arduino.h"
enum IPType { IPv4, IPv6 };
class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d) : bytes_{a,b,c,d} {}
  explicit IPAddress(const uint8_t *value) { memcpy(bytes_,value,4); }
  uint8_t operator[](unsigned index) const { return bytes_[index]; }
  explicit operator uint32_t() const {
    return uint32_t(bytes_[0])<<24|uint32_t(bytes_[1])<<16|uint32_t(bytes_[2])<<8|bytes_[3];
  }
  IPType type() const { return kind; }
  bool operator==(const IPAddress &other) const { return kind==other.kind && memcmp(bytes_,other.bytes_,4)==0; }
  bool operator!=(const IPAddress &other) const { return !(*this==other); }
  IPType kind=IPv4;
 private:
  uint8_t bytes_[4]{};
};
