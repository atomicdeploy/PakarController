#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace BoardCore { namespace BleWire {
// GATT is a packet transport, not an RPC receipt. Adapt the donor link/session
// consume-before-dispatch and single-inflight discipline without replaying an
// action when a notification is lost. Only identical fragments may be resent.
constexpr size_t kHeader = 7, kPacket = 512, kRequest = 8192;
constexpr uint32_t kPartialMs = 5000, kRetryMs = 700;
constexpr uint8_t kRetries = 3;
enum Kind : uint8_t { Data = 0, Ack = 1 };
inline uint16_t get16(const uint8_t *p) { return p[0] | static_cast<uint16_t>(p[1]) << 8; }
inline void put16(uint8_t *p,uint16_t v) { p[0]=static_cast<uint8_t>(v);p[1]=static_cast<uint8_t>(v>>8); }
inline void header(uint8_t *p,Kind kind,uint16_t id,uint16_t offset,uint16_t total) {
  p[0]=kind;put16(p+1,id);put16(p+3,offset);put16(p+5,total);
}
struct Frame { Kind kind=Data;uint16_t id=0,offset=0,total=0;const uint8_t *data=nullptr;size_t length=0; };
inline bool parse(const uint8_t *p,size_t length,Frame &f) {
  if(!p || length<kHeader || length>kPacket || p[0]>Ack)return false;
  f.kind=static_cast<Kind>(p[0]);f.id=get16(p+1);f.offset=get16(p+3);f.total=get16(p+5);
  f.data=p+kHeader;f.length=length-kHeader;
  return f.id && f.total && f.offset<=f.total &&
    (f.kind==Ack ? f.length==0 : f.length && f.length<=static_cast<size_t>(f.total-f.offset));
}
inline size_t packetLimit(uint16_t mtu) {
  const size_t value=mtu<23?20:static_cast<size_t>(mtu)-3;
  return value>kPacket?kPacket:value;
}
class Receiver {
 public:
  enum Result { Rejected, Accepted, Duplicate, Complete };
  void reset() { active_=ready_=lastValid_=false;next_=id_=total_=0; }
  void expire(uint32_t now) { if(active_ && (static_cast<uint32_t>(now-updated_)>=kPartialMs ||
    static_cast<uint32_t>(now-started_)>=60000U))reset(); }
  Result accept(const uint8_t *packet,size_t length,uint32_t now,uint8_t ack[kHeader]) {
    expire(now);Frame f;if(!parse(packet,length,f)||f.kind!=Data||f.total>kRequest)return Rejected;
    if(lastValid_ && f.id==id_ && f.total==total_ && f.offset==lastOffset_ &&
       f.length==lastLength_ && !memcmp(buffer_+lastOffset_,f.data,f.length)) {
      header(ack,Ack,id_,next_,total_);updated_=now;return Duplicate;
    }
    // A completed wire ID has one immutable payload. Only its exact final
    // fragment may be retried; changed bytes/length cannot create a new RPC.
    if(!active_&&lastValid_&&f.id==id_)return Rejected;
    if(!active_) {
      if(f.offset || ready_)return Rejected;
      active_=true;id_=f.id;total_=f.total;next_=0;lastValid_=false;started_=now;
    }
    if(f.id!=id_||f.total!=total_||f.offset!=next_||ready_)return Rejected;
    memcpy(buffer_+next_,f.data,f.length);lastOffset_=next_;lastLength_=f.length;lastValid_=true;
    next_=static_cast<uint16_t>(next_+f.length);updated_=now;
    header(ack,Ack,id_,next_,total_);
    if(next_==total_) { active_=false;ready_=true;return Complete; }
    return Accepted;
  }
  bool ready() const { return ready_; }
  const uint8_t *data() const { return buffer_; }
  uint16_t size() const { return total_; }
  uint16_t id() const { return id_; }
  // Preserve the exact final fragment for a lost-ACK duplicate. A different
  // message must start at zero; it still passes the application's intent cache.
  void consumed() { ready_=false; }
 private:
  uint8_t buffer_[kRequest]{};
  uint16_t id_=0,total_=0,next_=0,lastOffset_=0;
  size_t lastLength_=0;uint32_t updated_=0,started_=0;
  bool active_=false,ready_=false,lastValid_=false;
};
class Sender {
 public:
  void reset() { data_=nullptr;total_=next_=id_=0;waiting_=failed_=false; }
  bool begin(uint16_t id,const uint8_t *data,size_t size) {
    if(busy()||!id||!data||!size||size>65535)return false;
    reset();id_=id;data_=data;total_=static_cast<uint16_t>(size);return true;
  }
  bool busy() const { return data_!=nullptr; }
  bool failed() const { return failed_; }
  // Returned packet storage remains unchanged until its exact ACK. MTU changes
  // cannot resize an outstanding fragment and accidentally repeat byte ranges.
  const uint8_t *poll(uint32_t now,uint16_t mtu,size_t &length) {
    length=0;if(!busy())return nullptr;
    if(waiting_) {
      if(static_cast<uint32_t>(now-sent_)<kRetryMs)return nullptr;
      if(attempts_>=kRetries) { data_=nullptr;failed_=true;return nullptr; }
    } else {
      const size_t room=packetLimit(mtu)-kHeader;
      const size_t remaining=total_-next_;const size_t amount=remaining<room?remaining:room;
      header(packet_,Data,id_,next_,total_);memcpy(packet_+kHeader,data_+next_,amount);
      packetLength_=kHeader+amount;end_=static_cast<uint16_t>(next_+amount);attempts_=0;waiting_=true;
    }
    sent_=now;++attempts_;length=packetLength_;return packet_;
  }
  bool acknowledge(const uint8_t *packet,size_t length) {
    Frame f;if(!parse(packet,length,f)||f.kind!=Ack||!busy()||!waiting_||
      f.id!=id_||f.total!=total_||f.offset!=end_)return false;
    next_=end_;waiting_=false;if(next_==total_)data_=nullptr;return true;
  }
 private:
  const uint8_t *data_=nullptr;uint8_t packet_[kPacket]{};
  uint16_t id_=0,total_=0,next_=0,end_=0;size_t packetLength_=0;
  uint32_t sent_=0;uint8_t attempts_=0;bool waiting_=false,failed_=false;
};
} } // namespace BoardCore::BleWire
