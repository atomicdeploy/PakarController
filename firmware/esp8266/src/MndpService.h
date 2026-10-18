#pragma once
#if defined(ESP32)
#include <WiFi.h>
#include "../../esp32s3/src/SsdpUdp.h"
#include "../../esp32s3/src/DiscoveryInterfaces.h"
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>
#include <BuildInfo.h>
#include "Mndp.h"

#ifndef PAKAR_ENABLE_MNDP
#define PAKAR_ENABLE_MNDP 1
#endif

class MndpService {
 public:
  void stop() {
    udp_.stop();running_=false;
#if defined(ESP32)
    seenCount_=0;retryAt_=0;
    for (unsigned i=0;i<3;++i) {seen_[i]=0;joined_[i]=false;}
#endif
  }
  bool begin() {
    stop();last_=millis()-30000UL;
#if defined(ESP32)
    refreshInterfaces();
#else
    running_=PAKAR_ENABLE_MNDP && udp_.begin(5678);
#endif
    return running_;
  }
  bool running() const {return running_;}
  void service(const char *hostname,const char *boardId) {
#if defined(ESP32)
    refreshInterfaces();
#endif
    if(!running_)return;
    const int length=udp_.parsePacket();
    if(length>0) {
      const IPAddress source=udp_.remoteIP();const uint16_t port=udp_.remotePort();
      uint8_t request[4]{};
      const int read=udp_.read(request,sizeof(request));
      // Drain at most one incoming datagram; never loop over LAN input forever.
#if defined(ESP32)
      udp_.clear();
#else
      udp_.flush();
#endif
      if(length==4 && read==4 && request[0]==0 && request[1]==0 &&
         request[2]==0 && request[3]==0 && millis()-replyAt_>=1000) {
        replyAt_=millis();send(hostname,boardId,source,port,false);
      }
    }
    if(static_cast<uint32_t>(millis()-last_)>=30000) {
      last_=millis();send(hostname,boardId,IPAddress(224,0,0,88),5678,true);
    }
  }
 private:
  void send(const char *hostname,const char *boardId,const IPAddress &target,uint16_t port,bool multicast,bool forceAp=false) {
#if defined(ESP32)
    (void)forceAp;
    const auto interfaces=PakarS3::activeDiscoveryInterfaces();
    if (multicast) {
      for (uint8_t i=0;i<interfaces.count;++i)
        sendInterface(hostname,boardId,target,port,true,interfaces.entries[i]);
    } else if (const auto *selected=interfaces.forRemote(PakarS3::discoveryAddress(target))) {
      sendInterface(hostname,boardId,target,port,false,*selected);
    }
#else
    PakarMndp::Packet packet(sequence_++);
    const IPAddress ap=WiFi.softAPIP();
    bool targetOnAp = !multicast && ap != IPAddress();
    for(unsigned i=0;targetOnAp && i<3;++i)targetOnAp=target[i]==ap[i];
    const bool station = !forceAp && !targetOnAp && WiFi.status() == WL_CONNECTED;
    uint8_t mac[6];if(station)WiFi.macAddress(mac);else WiFi.softAPmacAddress(mac);packet.tlv(1,mac,6);
    packet.text(5,hostname);packet.text(7,BoardCore::BuildInfo::kVersion);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    packet.text(8,"ESP32-S3");
#elif defined(ESP32)
    packet.text(8,"ESP32");
#else
    packet.text(8,"ESP8266");
#endif
    packet.text(12,"PakarController");
    packet.text(11,boardId);packet.text(16,station ? "station" : "access-point");
    const uint32_t seconds=millis()/1000;
    const uint8_t uptime[4]={static_cast<uint8_t>(seconds),static_cast<uint8_t>(seconds>>8),
      static_cast<uint8_t>(seconds>>16),static_cast<uint8_t>(seconds>>24)};
    packet.tlv(10,uptime,4); // MNDP uptime is little-endian.
    const IPAddress ip=station ? WiFi.localIP() : WiFi.softAPIP();const uint8_t address[4]={ip[0],ip[1],ip[2],ip[3]};packet.tlv(17,address,4);
    if(!packet.size())return;
    const bool opened=multicast ? udp_.beginPacketMulticast(target,port,ip) : udp_.beginPacket(target,port);
    if(opened) {udp_.write(packet.data(),packet.size());udp_.endPacket();}
#endif
  }
#if defined(ESP32)
  void refreshInterfaces() {
    const auto interfaces=PakarS3::activeDiscoveryInterfaces();
    bool changed=seenCount_!=interfaces.count;
    for (uint8_t i=0;i<interfaces.count;++i)
      if (seen_[i]!=interfaces.entries[i].address) changed=true;
    if (changed) {
      udp_.stop();running_=false;seenCount_=interfaces.count;retryAt_=0;
      for (uint8_t i=0;i<3;++i) {seen_[i]=i<interfaces.count?interfaces.entries[i].address:0;joined_[i]=false;}
    }
    if (!PAKAR_ENABLE_MNDP || !interfaces.count) return;
    const uint32_t now=millis();
    if (retryAt_ && static_cast<int32_t>(now-retryAt_)<0) return;
    if (!running_) running_=udp_.begin(5678);
    bool retry=!running_;
    if (running_) for (uint8_t i=0;i<interfaces.count;++i) {
      if (!joined_[i]) joined_[i]=udp_.joinMulticast(
          PakarS3::discoveryAddress(interfaces.entries[i].address),IPAddress(224,0,0,88));
      if (!joined_[i]) retry=true;
    }
    retryAt_=retry?now+1000UL:0;
  }
  void sendInterface(const char *hostname,const char *boardId,const IPAddress &target,
      uint16_t port,bool multicast,const PakarS3::DiscoveryInterface &interface) {
    PakarMndp::Packet packet(sequence_++);
    packet.tlv(1,interface.mac,6); packet.text(5,hostname);
    packet.text(7,BoardCore::BuildInfo::kVersion);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    packet.text(8,"ESP32-S3");
#else
    packet.text(8,"ESP32");
#endif
    packet.text(12,"PakarController");packet.text(11,boardId);packet.text(16,interface.kind);
    const uint32_t seconds=millis()/1000;
    const uint8_t uptime[4]={static_cast<uint8_t>(seconds),static_cast<uint8_t>(seconds>>8),
      static_cast<uint8_t>(seconds>>16),static_cast<uint8_t>(seconds>>24)};
    packet.tlv(10,uptime,4);
    const IPAddress ip=PakarS3::discoveryAddress(interface.address);
    const uint8_t address[4]={ip[0],ip[1],ip[2],ip[3]};packet.tlv(17,address,4);
    if(!packet.size())return;
    const bool opened=multicast?udp_.beginPacketMulticast(target,port,ip):udp_.beginPacket(target,port);
    if(opened){udp_.write(packet.data(),packet.size());udp_.endPacket();}
  }
  uint32_t seen_[3]{},retryAt_=0;
  uint8_t seenCount_=0;
  bool joined_[3]{};
  PakarS3::SsdpUdp udp_;
#else
  WiFiUDP udp_;
#endif
  bool running_=false;uint16_t sequence_=0;uint32_t last_=0,replyAt_=0;
};
