#pragma once
#include "WiFi.h"
struct NbnsFixtureEthernet {
  bool ready=false;IPAddress address,mask{255,255,255,0};const char* name="en1";
  bool started()const{return ready;}bool connected()const{return ready;}bool linkUp()const{return ready;}bool hasIP()const{return ready;}
  IPAddress localIP()const{return address;}IPAddress subnetMask()const{return mask;}String impl_name()const{return name;}
  void macAddress(uint8_t* out)const{const uint8_t mac[6]={2,0,0,0,0,3};memcpy(out,mac,6);}
};
inline NbnsFixtureEthernet ETH;
