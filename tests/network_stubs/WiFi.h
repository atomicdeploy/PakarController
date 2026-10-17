#pragma once
#include "Arduino.h"
#include <cstdio>
class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d) : bytes_{a,b,c,d} {}
  explicit IPAddress(const uint8_t *bytes) { memcpy(bytes_,bytes,4); }
  uint8_t operator[](unsigned index) const { return bytes_[index]; }
  bool fromString(const String &value) {
    unsigned parts[4]; char trailing;
    if (sscanf(value.c_str(), "%u.%u.%u.%u%c", &parts[0], &parts[1], &parts[2], &parts[3], &trailing) != 4) return false;
    for(unsigned i=0;i<4;++i) { if(parts[i]>255)return false; bytes_[i]=parts[i]; }
    return true;
  }
  String toString() const {
    return String(std::to_string(bytes_[0])+"."+std::to_string(bytes_[1])+"."+
        std::to_string(bytes_[2])+"."+std::to_string(bytes_[3]));
  }
 private:
  uint8_t bytes_[4]{};
};
enum {WIFI_OFF=0,WIFI_STA=1,WIFI_AP=2,WIFI_AP_STA=3,WL_CONNECTED=3};
struct NetworkFixtureWiFi {
  int modeValue=0, connection=0;
  bool persistentValue=true, apResult=true, configResult=true, autoReconnect=false;
  String hostnameValue, ssidValue, passwordValue;
  IPAddress address;
  void persistent(bool value){persistentValue=value;}
  bool mode(int value){modeValue=value;return true;}
  void disconnect(bool){}
  void softAPdisconnect(bool){}
  bool hostname(const char *value){hostnameValue=value;return true;}
  bool setHostname(const char *value){hostnameValue=value;return true;}
  bool softAP(const char *,const char *){return apResult;}
  bool config(IPAddress ip,IPAddress,IPAddress,IPAddress=IPAddress(),IPAddress=IPAddress()){address=ip;return configResult;}
  void begin(const char *ssid,const char *password){ssidValue=ssid;passwordValue=password;}
  void setAutoReconnect(bool value){autoReconnect=value;}
  void setAutoConnect(bool){}
  int status()const{return connection;}
};
extern NetworkFixtureWiFi WiFi;
