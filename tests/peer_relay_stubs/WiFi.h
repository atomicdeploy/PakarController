#pragma once
#include "Arduino.h"
enum IPType {IPv4,IPv6};
class IPAddress {
 public:
  IPAddress()=default;
  explicit IPAddress(uint32_t v):bytes_{uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)}{}
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes_{a,b,c,d}{}
  bool fromString(const String &s){unsigned a,b,c,d;char extra;
    if(sscanf(s.c_str(),"%u.%u.%u.%u%c",&a,&b,&c,&d,&extra)!=4||a>255||b>255||c>255||d>255)return false;
    bytes_[0]=a;bytes_[1]=b;bytes_[2]=c;bytes_[3]=d;return true;
  }
  String toString()const{char out[16];snprintf(out,sizeof(out),"%u.%u.%u.%u",bytes_[0],bytes_[1],bytes_[2],bytes_[3]);return out;}
  uint8_t operator[](unsigned i)const{return bytes_[i];}IPType type()const{return IPv4;}
  bool operator==(const IPAddress &other)const{return !memcmp(bytes_,other.bytes_,4);}
  bool operator!=(const IPAddress &other)const{return !(*this==other);}
 private:uint8_t bytes_[4]{};
};
struct TestWiFi {IPAddress localIP()const{return {192,168,1,218};}IPAddress softAPIP()const{return {192,168,4,1};}IPAddress gatewayIP()const{return {192,168,4,1};}};
inline TestWiFi WiFi;
inline String physicalAddress;
struct NetworkClient {
  virtual ~NetworkClient()=default;
  virtual int connect(const char*,uint16_t){throw std::runtime_error("unexpected DNS dial");}
  virtual int connect(const char*,uint16_t,int32_t){throw std::runtime_error("unexpected DNS dial");}
  virtual int connect(IPAddress ip,uint16_t,int32_t){physicalAddress=ip.toString();return 1;}
};
