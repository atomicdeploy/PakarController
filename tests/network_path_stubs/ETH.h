#pragma once
#define CONFIG_ETH_ENABLED 1
#define CONFIG_ETH_SPI_ETHERNET_W5500 1
#include "WiFi.h"
#include "SPI.h"
constexpr int ETH_PHY_W5500=7;
struct PathFixtureEthernet {
  bool start=false,carrier=false,up=false,ipKnown=false,defaultResult=true;
  void *driver=nullptr,*interface=nullptr;
  IPAddress address,gateway,mask{255,255,255,0},dns[2];
  unsigned begins=0,ends=0,configs=0,defaults=0,dnsWrites=0;
  bool begin(int,int,int,int,int,SPIClass&,uint8_t){++begins;driver=interface=this;return true;}
  void end(){++ends;driver=interface=nullptr;start=carrier=up=ipKnown=false;}
  void *handle()const{return driver;}
  void *netif()const{return interface;}
  bool setHostname(const char*){return interface!=nullptr;}
  bool config(IPAddress ip=IPAddress(),IPAddress gw=IPAddress(),IPAddress sm=IPAddress(),
      IPAddress first=IPAddress(),IPAddress second=IPAddress()){
    ++configs;address=ip;gateway=gw;mask=sm;dns[0]=first;dns[1]=second;return true;
  }
  bool started()const{return start;}
  bool connected()const{return carrier;}
  bool linkUp()const{return up;}
  bool hasIP()const{return ipKnown;}
  bool setDefault(){++defaults;return defaultResult;}
  IPAddress localIP()const{return address;}
  IPAddress gatewayIP()const{return gateway;}
  IPAddress subnetMask()const{return mask;}
  IPAddress dnsIP(uint8_t index)const{return index<2?dns[index]:IPAddress();}
  bool dnsIP(uint8_t index,IPAddress value){++dnsWrites;if(index>=2)return false;dns[index]=value;return true;}
  String macAddress()const{return "02:00:00:00:00:03";}
  void macAddress(uint8_t *out)const{const uint8_t value[6]={2,0,0,0,0,3};memcpy(out,value,6);}
};
inline PathFixtureEthernet ETH;
