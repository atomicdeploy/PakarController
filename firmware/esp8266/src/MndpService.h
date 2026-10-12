#pragma once
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <BuildInfo.h>
#include "Mndp.h"

#ifndef PAKAR_ENABLE_MNDP
#define PAKAR_ENABLE_MNDP 1
#endif

class MndpService {
 public:
  void stop() {udp_.stop();running_=false;}
  bool begin() {stop();running_=PAKAR_ENABLE_MNDP && udp_.begin(5678);last_=millis()-30000UL;return running_;}
  bool running() const {return running_;}
  void service(const char *hostname,const char *boardId) {
    if(!running_)return;
    const int length=udp_.parsePacket();
    if(length>0) {
      const IPAddress source=udp_.remoteIP();const uint16_t port=udp_.remotePort();
      uint8_t request[4]{};
      const int read=udp_.read(request,sizeof(request));
      // Drain at most one incoming datagram; never loop over LAN input forever.
      udp_.flush();
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
  void send(const char *hostname,const char *boardId,const IPAddress &target,uint16_t port,bool multicast) {
    PakarMndp::Packet packet(sequence_++);
    uint8_t mac[6];WiFi.macAddress(mac);packet.tlv(1,mac,6);
    packet.text(5,hostname);packet.text(7,BoardCore::BuildInfo::kVersion);
    packet.text(8,"ESP8266");packet.text(12,"PakarController");
    packet.text(11,boardId);packet.text(16,"station");
    const uint32_t seconds=millis()/1000;
    const uint8_t uptime[4]={static_cast<uint8_t>(seconds),static_cast<uint8_t>(seconds>>8),
      static_cast<uint8_t>(seconds>>16),static_cast<uint8_t>(seconds>>24)};
    packet.tlv(10,uptime,4); // MNDP uptime is little-endian.
    const IPAddress ip=WiFi.localIP();const uint8_t address[4]={ip[0],ip[1],ip[2],ip[3]};packet.tlv(17,address,4);
    if(!packet.size())return;
    const bool opened=multicast ? udp_.beginPacketMulticast(target,port,WiFi.localIP()) : udp_.beginPacket(target,port);
    if(opened) {udp_.write(packet.data(),packet.size());udp_.endPacket();}
  }
  WiFiUDP udp_;bool running_=false;uint16_t sequence_=0;uint32_t last_=0,replyAt_=0;
};
