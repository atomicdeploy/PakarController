#pragma once
#include <Arduino.h>
#include <Protocol.h>
#include <stdint.h>

// Explicit finite USB observation lease, not ownership or permission to mutate.
// No HELLO/frame counter means a permanently connected consumer. As with donor
// status subscriptions, cancelling the last consumer retires background work.
class HostDemand {
 public:
  bool configure(uint16_t flags,uint16_t lease,uint32_t now) {
    clear();
    using namespace BoardCore::Protocol::UsbDemand;
    if(flags&~kAllowedFlags)return false;
    if(!flags)return lease==0;
    if(lease<kMinimumLeaseMs || lease>kMaximumLeaseMs)return false;
    flags_=flags;until_=now+lease;return true;
  }
  void clear(){flags_=0;until_=0;}
  uint16_t remaining(uint32_t now) {
    if(!flags_ || static_cast<int32_t>(until_-now)<=0){clear();return 0;}
    return static_cast<uint16_t>(until_-now);
  }
  uint16_t flags(uint32_t now){return remaining(now)?flags_:0;}
  bool wants(uint16_t flag,uint32_t now){return (flags(now)&flag)!=0;}
 private:
  uint32_t until_=0;uint16_t flags_=0;
};

// Source-independent transport wrapper allows the same short-write retirement
// policy to be exercised with a real SerialLink and host Stream test double.
template<class Port> class HostSerialAdapter : public Stream {
 public:
  HostSerialAdapter(Port &port,HostDemand &demand):port_(port),demand_(demand){}
  int available()override{return port_.available();}
  int read()override{return port_.read();}
  int peek(){return port_.peek();}
  void flush(){port_.flush();}
  int availableForWrite(){return port_.availableForWrite();}
  size_t write(uint8_t byte){return write(&byte,1);}
  size_t write(const uint8_t *data,size_t bytes)override{
    const size_t written=port_.write(data,bytes);
    if(written!=bytes)demand_.clear(); // Unknown outcome is never a lease/replay.
    return written;
  }
 private:
  Port &port_;HostDemand &demand_;
};
