#pragma once
#include "IPAddress.h"
enum {WIFI_OFF=0,WIFI_STA=1,WIFI_AP=2,WIFI_AP_STA=3,WL_CONNECTED=3};
struct NbnsFixtureWifi {
  int connection=0,mode=WIFI_OFF;IPAddress address,mask{255,255,255,0},ap,apMask{255,255,255,0};
  struct Interface {const char* name="";String impl_name()const{return name;}} STA{"st1"},AP{"ap1"};
  int status()const{return connection;}int getMode()const{return mode;}
  IPAddress localIP()const{return address;}IPAddress subnetMask()const{return mask;}
  IPAddress softAPIP()const{return ap;}IPAddress softAPSubnetMask()const{return apMask;}
  void macAddress(uint8_t* out)const{const uint8_t mac[6]={2,0,0,0,0,1};memcpy(out,mac,6);}
  void softAPmacAddress(uint8_t* out)const{const uint8_t mac[6]={2,0,0,0,0,2};memcpy(out,mac,6);}
};
inline NbnsFixtureWifi WiFi;
