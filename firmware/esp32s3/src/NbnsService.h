#pragma once
#include "DiscoveryInterfaces.h"
#include "NbnsWire.h"
#include <algorithm>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <unistd.h>

namespace PakarS3 {
// PU850's NBNS begin/hostname lifecycle is retained, not its unused blocking
// NetDiscovery sample. Three nonblocking, interface-bound sockets replace the
// SDK callback parser: IP_PKTINFO is OFF in the shipping S3 SDK. SO_BINDTODEVICE
// binds lwIP UDP receive AND send to the actual netif, even on overlapping LANs.
class NbnsService {
 public:
  NbnsService()=default;
  NbnsService(const NbnsService&)=delete;
  NbnsService& operator=(const NbnsService&)=delete;
  ~NbnsService(){end();}
  bool begin(const char* hostname){
    end();
    if(!PakarNbns::hostname(hostname,name_)){state_="invalid-or-long-hostname";return false;}
    configured_=true;refresh();return running();
  }
  void end(){
    closeSlots();seen_=DiscoveryInterfaces{};configured_=false;name_[0]=0;state_="stopped";
  }
  bool running()const{return boundInterfaces()!=0;}
  uint8_t boundInterfaces()const{uint8_t n=0;for(const auto& slot:slots_)if(slot.fd>=0)++n;return n;}
  const char* state()const{return state_;}
  const char* name()const{return name_;}
  uint32_t replies()const{return replies_;}
  uint32_t rejected()const{return rejected_;}
  uint32_t sendFailures()const{return sendFailures_;}
  void service(){
    if(!configured_)return;
    refresh();
    for(uint8_t i=0;i<seen_.count;++i){
      auto& slot=slots_[i];if(slot.fd<0)continue;
      uint8_t query[PakarNbns::kQuestionBytes+1],reply[PakarNbns::kMaximumReply];
      sockaddr_in peer{};socklen_t peerSize=sizeof(peer);
      // Exactly one datagram/interface/pass; truncation never becomes a query.
      const int size=recvfrom(slot.fd,query,sizeof(query),MSG_DONTWAIT,reinterpret_cast<sockaddr*>(&peer),&peerSize);
      if(size<=0)continue;
      const auto& interface=seen_.entries[i];const uint32_t remote=ntohl(peer.sin_addr.s_addr),now=millis();
      const uint32_t elapsed=now-slot.refillAt;
      if(elapsed>=100U){slot.credit=uint8_t(std::min<uint32_t>(2U,slot.credit+elapsed/100U));slot.refillAt=now;}
      if(peer.sin_family!=AF_INET||!ntohs(peer.sin_port)||!localPeer(remote,interface)||!slot.credit){++rejected_;continue;}
      const size_t bytes=PakarNbns::answer(query,size,name_,interface.address,interface.mac,reply,sizeof(reply));
      if(!bytes){++rejected_;continue;}
      --slot.credit; // A query + NBSTAT burst is allowed, not unlimited reflection.
      // Preserve request source port; never broadcast answers or route through
      // the default uplink. The descriptor remains bound to ingress netif.
      const int written=sendto(slot.fd,reply,bytes,MSG_DONTWAIT,reinterpret_cast<sockaddr*>(&peer),peerSize);
      if(written==int(bytes))++replies_;else ++sendFailures_; // No queued retry.
    }
  }
 private:
  struct Slot {int fd=-1;uint32_t retryAt=0,refillAt=0;bool retry=false;uint8_t credit=2;char device[8]{};};
  static bool localPeer(uint32_t remote,const DiscoveryInterface& interface){
    const uint8_t first=uint8_t(remote>>24);
    if(!first||first==127||first>=224||remote==interface.address||!interface.mask||
        (remote&interface.mask)!=(interface.address&interface.mask))return false;
    const uint32_t host=remote&~interface.mask;
    return interface.mask>=0xfffffffeUL||(host&&host!=(~interface.mask));
  }
  void closeSlots(){for(auto& slot:slots_){if(slot.fd>=0)close(slot.fd);slot=Slot{};}}
  static String deviceName(const char* kind){
#if defined(PAKAR_S3_ETHERNET_ENABLED) && PAKAR_S3_ETHERNET_ENABLED
    if(!strcmp(kind,"ethernet"))return ETH.impl_name();
#endif
    return !strcmp(kind,"station")?WiFi.STA.impl_name():!strcmp(kind,"access-point")?WiFi.AP.impl_name():String();
  }
  bool open(Slot& slot){
    if(!slot.device[0])return false;
    struct ifreq device{};
    if(strlen(slot.device)>=sizeof(device.ifr_name))return false;
    memcpy(device.ifr_name,slot.device,strlen(slot.device)+1);
    slot.fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(slot.fd<0)return false;
    const int enabled=1;
    sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=INADDR_ANY;local.sin_port=htons(137);
    const int flags=fcntl(slot.fd,F_GETFL,0);
    if(flags<0||setsockopt(slot.fd,SOL_SOCKET,SO_REUSEADDR,&enabled,sizeof(enabled))||
        setsockopt(slot.fd,SOL_SOCKET,SO_BROADCAST,&enabled,sizeof(enabled))||
        setsockopt(slot.fd,SOL_SOCKET,SO_BINDTODEVICE,&device,sizeof(device))||
        bind(slot.fd,reinterpret_cast<sockaddr*>(&local),sizeof(local))||fcntl(slot.fd,F_SETFL,flags|O_NONBLOCK)){
      close(slot.fd);slot.fd=-1;return false;
    }
    return true;
  }
  void refresh(){
    const auto interfaces=activeDiscoveryInterfaces();bool changed=interfaces.count!=seen_.count;
    for(uint8_t i=0;i<interfaces.count&&!changed;++i){
      const auto& a=interfaces.entries[i];const auto& b=seen_.entries[i];
      changed=a.address!=b.address||a.mask!=b.mask||memcmp(a.mac,b.mac,6)||strcmp(a.kind,b.kind);
    }
    if(changed){
      closeSlots();seen_=interfaces;
      for(uint8_t i=0;i<seen_.count;++i){
        const String device=deviceName(seen_.entries[i].kind);
        if(device.length()&&device.length()<sizeof(slots_[i].device))memcpy(slots_[i].device,device.c_str(),device.length()+1);
        slots_[i].refillAt=millis();
      }
    }
    if(!seen_.count){state_="no-active-interfaces";return;}
    const uint32_t now=millis();
    for(uint8_t i=0;i<seen_.count;++i){auto& slot=slots_[i];
      if(slot.fd>=0||(slot.retry&&int32_t(now-slot.retryAt)<0))continue;
      slot.retry=!open(slot);slot.retryAt=now+1000U;
    }
    const uint8_t bound=boundInterfaces();state_=bound==seen_.count?"ready":bound?"partial-bind":"socket-bind-failed";
  }
  DiscoveryInterfaces seen_{};Slot slots_[3];char name_[16]{};
  bool configured_=false;const char* state_="stopped";
  uint32_t replies_=0,rejected_=0,sendFailures_=0;
};
} // namespace PakarS3
