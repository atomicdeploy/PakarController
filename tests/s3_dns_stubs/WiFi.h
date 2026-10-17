#pragma once
#include <cstdint>
enum IPType {IPv4,IPv6};
class IPAddress {
 public:
  IPAddress():value_(0),type_(IPv4){}
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):value_(a|(uint32_t(b)<<8)|(uint32_t(c)<<16)|(uint32_t(d)<<24)),type_(IPv4){}
  explicit IPAddress(IPType type):value_(0),type_(type){}
  IPType type() const{return type_;}
  uint8_t operator[](unsigned i) const{return (value_>>(i*8))&255;}
  explicit operator uint32_t() const{return value_;}
  bool operator==(const IPAddress &o) const{return value_==o.value_ && type_==o.type_;}
 private:uint32_t value_;IPType type_;
};
constexpr int WL_CONNECTED=3;
struct StaFixture {
 bool globalV6=false,fail=false;unsigned writes=0;IPAddress dns[2];
 bool hasGlobalIPv6() const{return globalV6;}
 bool dnsIP(uint8_t index,IPAddress value){++writes;if(fail)return false;dns[index]=value;return true;}
};
struct WifiFixture {
 StaFixture STA;int state=WL_CONNECTED;IPAddress gateway{192,168,1,1};
 int status()const{return state;}
 IPAddress dnsIP(unsigned index)const{return STA.dns[index];}
 IPAddress gatewayIP()const{return gateway;}
};
inline WifiFixture WiFi;
