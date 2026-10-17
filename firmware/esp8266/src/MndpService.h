#pragma once
#if defined(ESP32)
#include <WiFi.h>
#include "../../esp32s3/src/SsdpUdp.h"
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
  void stop() {udp_.stop();running_=false;}
  bool begin() {
    stop();running_=PAKAR_ENABLE_MNDP && udp_.begin(5678);last_=millis()-30000UL;
#if defined(ESP32)
    if (running_) {
      if (WiFi.status() == WL_CONNECTED) udp_.joinMulticast(WiFi.localIP(), IPAddress(224,0,0,88));
      if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
        udp_.joinMulticast(WiFi.softAPIP(), IPAddress(224,0,0,88));
    }
#endif
    return running_;
  }
  bool running() const {return running_;}
  void service(const char *hostname,const char *boardId) {
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
#if defined(ESP32)
      if (WiFi.status() == WL_CONNECTED && (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA))
        send(hostname,boardId,IPAddress(224,0,0,88),5678,true,true);
#endif
    }
  }
 private:
  void send(const char *hostname,const char *boardId,const IPAddress &target,uint16_t port,bool multicast,bool forceAp=false) {
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
  }
#if defined(ESP32)
  PakarS3::SsdpUdp udp_;
#else
  WiFiUDP udp_;
#endif
  bool running_=false;uint16_t sequence_=0;uint32_t last_=0,replyAt_=0;
};
