#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// Bounded IEEE802.1AB diagnostics, never product identity/authorization. Only
// nearest-bridge Ethernet LLDP is accepted; unknown optional TLVs are skipped.
namespace PakarS3 {
namespace Lldp {
constexpr uint8_t destination[6]={1,0x80,0xc2,0,0,0x0e};
constexpr unsigned maxNeighbors=4;
struct Neighbor {
  uint8_t mac[6]{};
  uint8_t chassisSubtype=0,portSubtype=0;
  uint16_t ttl=0;
  uint32_t observedAt=0;
  uint64_t key=0; // Complete identifier fingerprint, not a security identity.
  char chassis[65]{},port[65]{},systemName[64]{},description[96]{};
};
inline uint16_t be16(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline bool candidate(const uint8_t *p,size_t n,size_t &offset) {
  if(!p||n<14||n>1522||memcmp(p,destination,6))return false;
  offset=14;uint16_t type=be16(p+12);
  for(unsigned tags=0;tags<2&&(type==0x8100||type==0x88a8);++tags) {
    if(n<offset+4)return false;
    type=be16(p+offset+2);offset+=4;
  }
  return type==0x88cc;
}
inline void text(char *out,size_t capacity,const uint8_t *p,size_t n) {
  if(!capacity)return;
  const size_t copied=n<capacity-1?n:capacity-1;
  for(size_t i=0;i<copied;++i)out[i]=p[i]>=32&&p[i]<=126?char(p[i]):'?';
  out[copied]=0;
}
inline uint64_t fingerprint(uint64_t hash,const uint8_t *p,size_t n) {
  for(size_t i=0;i<n;++i)hash=(hash^p[i])*UINT64_C(1099511628211);
  return hash;
}
inline void identifier(char out[65],uint8_t subtype,const uint8_t *p,size_t n,bool chassis) {
  if(n==6&&subtype==(chassis?4:3)) {
    snprintf(out,65,"%02x:%02x:%02x:%02x:%02x:%02x",p[0],p[1],p[2],p[3],p[4],p[5]);
  } else text(out,65,p,n);
}
inline bool parse(const uint8_t *frame,size_t length,uint32_t now,Neighbor &out) {
  size_t at=0;if(!candidate(frame,length,at))return false;
  bool any=false;for(unsigned i=0;i<6;++i)any=any||frame[6+i]!=0;
  if(!any||(frame[6]&1))return false;
  Neighbor parsed;memcpy(parsed.mac,frame+6,6);parsed.observedAt=now;
  parsed.key=UINT64_C(14695981039346656037);
  unsigned mandatory=0;bool named=false,described=false;
  for(unsigned count=0;count<32&&at+2<=length;++count) {
    const uint16_t header=be16(frame+at);at+=2;
    const unsigned type=header>>9,n=header&511;
    if(n>length-at)return false;
    const uint8_t *value=frame+at;at+=n;
    if(type==0) {
      if(n||mandatory!=3)return false;
      out=parsed;return true; // Ethernet padding after End is not another TLV.
    }
    if(mandatory<3&&type!=mandatory+1)return false;
    if(type==1||type==2) {
      if(mandatory!=type-1||n<2||n>256||value[0]<1||value[0]>7)return false;
      if(value[0]==(type==1?4:3)&&n!=7)return false;
      parsed.key=fingerprint(parsed.key,frame+at-n-2,n+2);
      if(type==1) {
        parsed.chassisSubtype=value[0];identifier(parsed.chassis,value[0],value+1,n-1,true);
      } else {
        parsed.portSubtype=value[0];identifier(parsed.port,value[0],value+1,n-1,false);
      }
      ++mandatory;
    } else if(type==3) {
      if(mandatory!=2||n!=2)return false;
      parsed.ttl=be16(value);++mandatory;
    } else if(type==5) {
      if(named)return false;
      named=true;text(parsed.systemName,sizeof(parsed.systemName),value,n);
    } else if(type==6) {
      if(described)return false;
      described=true;text(parsed.description,sizeof(parsed.description),value,n);
    }
  }
  // IEEE802.1AB-2016 permits omitted End only at an exact complete TLV boundary.
  // A trailing partial header/value remains invalid; there is no resync scan.
  if(mandatory==3&&at==length){out=parsed;return true;}
  return false;
}
class Neighbors {
 public:
  void clear(){for(auto &entry:entries_)entry=Neighbor{};}
  void expire(uint32_t now) {
    for(auto &entry:entries_)if(entry.ttl&&uint32_t(now-entry.observedAt)>=uint32_t(entry.ttl)*1000U)entry=Neighbor{};
  }
  void observe(const Neighbor &value,uint32_t now) {
    expire(now);Neighbor *slot=nullptr;
    for(auto &entry:entries_)if(entry.ttl&&entry.key==value.key&&!memcmp(entry.mac,value.mac,6)){slot=&entry;break;}
    if(!value.ttl){if(slot)*slot=Neighbor{};return;}
    if(!slot)for(auto &entry:entries_)if(!entry.ttl){slot=&entry;break;}
    if(!slot) { // Bounded oldest observation eviction, never unbounded storage.
      slot=&entries_[0];for(auto &entry:entries_)if(uint32_t(now-entry.observedAt)>uint32_t(now-slot->observedAt))slot=&entry;
    }
    *slot=value;
  }
  unsigned copy(Neighbor out[maxNeighbors],uint32_t now)const {
    unsigned count=0;
    for(const auto &entry:entries_)if(entry.ttl&&uint32_t(now-entry.observedAt)<uint32_t(entry.ttl)*1000U)out[count++]=entry;
    return count;
  }
 private:
  Neighbor entries_[maxNeighbors]{};
};
inline size_t frame(uint8_t *out,size_t capacity,const uint8_t mac[6],const char *hostname,uint16_t ttl=120) {
  if(!out||!mac||!hostname||capacity<160)return 0;
  size_t nameLength=0;while(nameLength<64&&hostname[nameLength])++nameLength;
  if(!nameLength||nameLength>63)return 0;
  for(size_t i=0;i<nameLength;++i)if(hostname[i]<33||hostname[i]>126)return 0;
  bool any=false;for(unsigned i=0;i<6;++i)any=any||mac[i]!=0;
  if(!any||(mac[0]&1))return 0;
  size_t at=14;memcpy(out,destination,6);memcpy(out+6,mac,6);out[12]=0x88;out[13]=0xcc;
  const auto tlv=[&](uint8_t type,const uint8_t *p,size_t n) {
    const uint16_t h=uint16_t(type)<<9|uint16_t(n);out[at++]=h>>8;out[at++]=h;
    memcpy(out+at,p,n);at+=n;
  };
  uint8_t chassis[7]={4};memcpy(chassis+1,mac,6);tlv(1,chassis,7);
  const uint8_t port[]={5,'w','5','5','0','0'};tlv(2,port,sizeof(port));
  const uint8_t lifetime[]={uint8_t(ttl>>8),uint8_t(ttl)};tlv(3,lifetime,2);
  if(ttl) { // A shutdown LLDPDU must not carry ordinary optional TLVs.
    tlv(5,reinterpret_cast<const uint8_t*>(hostname),nameLength);
    const uint8_t description[]="PakarController ESP32-S3 W5500";tlv(6,description,sizeof(description)-1);
    const uint8_t capabilities[]={0,0x80,0,0x80};tlv(7,capabilities,4); // Station only, never a bridge/router.
  }
  out[at++]=0;out[at++]=0;
  while(at<60)out[at++]=0; // Driver supplies FCS, not this payload.
  return at;
}
} // namespace Lldp
struct LldpStatus {
  bool enabled=false,receiveRequested=false,receiveSupported=false,receiveActive=false,promiscuous=false;
  bool transmitSupported=false,jobPending=false;
  uint32_t transmitted=0,transmitFailed=0,received=0,invalid=0,dropped=0,queueRejected=0;
  int lastError=0;
  unsigned neighborCount=0;
  Lldp::Neighbor neighbors[Lldp::maxNeighbors]{};
};
} // namespace PakarS3
