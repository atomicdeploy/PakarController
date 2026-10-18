#pragma once
#include "IPAddress.h"
enum { WIFI_OFF=0,WIFI_STA=1,WIFI_AP=2,WIFI_AP_STA=3,WL_CONNECTED=3 };
struct PathFixtureWifi {
  int connection=0,mode=WIFI_OFF;
  IPAddress address,gateway,mask{255,255,255,0},ap,apMask{255,255,255,0},dns[2];
  unsigned begins=0,configs=0,disconnects=0;
  struct Station {
    bool defaultResult=true,globalV6=false;
    unsigned defaults=0,dnsWrites=0;
    bool setDefault(){++defaults;return defaultResult;}
    bool hasGlobalIPv6()const{return globalV6;}
    bool dnsIP(uint8_t index,IPAddress value);
  } STA;
  int status()const{return connection;}
  int getMode()const{return mode;}
  IPAddress localIP()const{return address;}
  IPAddress gatewayIP()const{return gateway;}
  IPAddress subnetMask()const{return mask;}
  IPAddress softAPIP()const{return ap;}
  IPAddress softAPSubnetMask()const{return apMask;}
  IPAddress dnsIP(uint8_t index)const{return index<2?dns[index]:IPAddress();}
  void macAddress(uint8_t *out)const{const uint8_t value[6]={2,0,0,0,0,1};memcpy(out,value,6);}
  void softAPmacAddress(uint8_t *out)const{const uint8_t value[6]={2,0,0,0,0,2};memcpy(out,value,6);}
  void begin(const char*,const char*){++begins;}
  bool config(IPAddress,IPAddress,IPAddress){++configs;return true;}
  void disconnect(){++disconnects;}
};
inline PathFixtureWifi WiFi;
inline bool PathFixtureWifi::Station::dnsIP(uint8_t index,IPAddress value){
  ++dnsWrites;if(index>=2)return false;WiFi.dns[index]=value;return true;
}
