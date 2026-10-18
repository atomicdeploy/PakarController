#include "../libraries/BoardCore/src/BleWire.h"
#include "../libraries/BoardCore/src/CommandReceipts.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include <limits>
using namespace BoardCore::BleWire;

static std::vector<uint8_t> data(uint16_t id,uint16_t offset,uint16_t total,const std::vector<uint8_t> &bytes) {
  std::vector<uint8_t> frame(kHeader+bytes.size());header(frame.data(),Data,id,offset,total);
  if(!bytes.empty())memcpy(frame.data()+kHeader,bytes.data(),bytes.size());
  return frame;
}
int main() {
  static_assert(kHeader==7 && kPacket==512 && kRequest==8192,"BLE wire bounds changed");
  assert(packetLimit(0)==20 && packetLimit(23)==20 && packetLimit(247)==244 && packetLimit(517)==512);
  {
    Frame decoded;uint8_t ack[kHeader];header(ack,Ack,3,10,10);
    assert(parse(ack,sizeof(ack),decoded)&&decoded.kind==Ack&&decoded.offset==10);
    ack[0]=2;assert(!parse(ack,sizeof(ack),decoded));
    header(ack,Ack,0,0,1);assert(!parse(ack,sizeof(ack),decoded));
    header(ack,Ack,1,2,1);assert(!parse(ack,sizeof(ack),decoded));
    auto empty=data(1,0,1,{});assert(!parse(empty.data(),empty.size(),decoded));
    auto tooMuch=data(1,0,1,{1,2});assert(!parse(tooMuch.data(),tooMuch.size(),decoded));
    assert(!parse(nullptr,0,decoded));
  }
  // Contiguous assembly, exact duplicate ACK, no dispatch on changed overlap,
  // gap, foreign message/total, truncated header or oversized request.
  {
    Receiver receiver;uint8_t ack[kHeader];
    const auto one=data(5,0,6,{1,2,3}),two=data(5,3,6,{4,5,6});
    assert(receiver.accept(one.data(),one.size(),10,ack)==Receiver::Accepted);
    assert(get16(ack+3)==3 && get16(ack+5)==6);
    assert(receiver.accept(one.data(),one.size(),11,ack)==Receiver::Duplicate);
    const auto changed=data(5,0,6,{7,2,3}),gap=data(5,4,6,{5,6}),foreign=data(6,3,6,{4,5,6});
    for(const auto &invalid:{changed,gap,foreign})assert(receiver.accept(invalid.data(),invalid.size(),12,ack)==Receiver::Rejected);
    auto wrongTotal=data(5,3,7,{4,5,6});assert(receiver.accept(wrongTotal.data(),wrongTotal.size(),13,ack)==Receiver::Rejected);
    assert(receiver.accept(two.data(),two.size(),14,ack)==Receiver::Complete);
    assert(receiver.ready()&&receiver.id()==5&&receiver.size()==6&&!memcmp(receiver.data(),"\1\2\3\4\5\6",6));
    assert(receiver.accept(two.data(),two.size(),15,ack)==Receiver::Duplicate);
    receiver.consumed();assert(!receiver.ready());
    assert(receiver.accept(two.data(),two.size(),16,ack)==Receiver::Duplicate&&!receiver.ready());
    auto changedCompleted=data(5,0,6,{9,9,9}),changedLength=data(5,0,1,{9});
    assert(receiver.accept(changedCompleted.data(),changedCompleted.size(),16,ack)==Receiver::Rejected);
    assert(receiver.accept(changedLength.data(),changedLength.size(),16,ack)==Receiver::Rejected);
    auto next=data(6,0,1,{9});assert(receiver.accept(next.data(),next.size(),17,ack)==Receiver::Complete);
    receiver.reset();auto large=data(7,0,8193,{9});assert(receiver.accept(large.data(),large.size(),18,ack)==Receiver::Rejected);
  }
  // Session fence / inactivity retires a prefix, including across millis wrap.
  {
    Receiver receiver;uint8_t ack[kHeader];const auto one=data(8,0,2,{1}),two=data(8,1,2,{2});
    const uint32_t start=std::numeric_limits<uint32_t>::max()-100U;
    assert(receiver.accept(one.data(),one.size(),start,ack)==Receiver::Accepted);
    receiver.expire(start+5000U);
    assert(receiver.accept(two.data(),two.size(),start+5001U,ack)==Receiver::Rejected);
    assert(receiver.accept(one.data(),one.size(),start+5002U,ack)==Receiver::Accepted);
    receiver.reset();assert(receiver.accept(two.data(),two.size(),start+5003U,ack)==Receiver::Rejected);
  }
  // A slow drip of exact duplicate fragments cannot retain assembly forever.
  {
    Receiver receiver;uint8_t ack[kHeader];const auto one=data(9,0,2,{1}),two=data(9,1,2,{2});
    assert(receiver.accept(one.data(),one.size(),100,ack)==Receiver::Accepted);
    for(uint32_t at=4100;at<60100;at+=4000)assert(receiver.accept(one.data(),one.size(),at,ack)==Receiver::Duplicate);
    receiver.expire(60100);assert(receiver.accept(two.data(),two.size(),60101,ack)==Receiver::Rejected);
  }
  // Full minimum-MTU request uses thirteen-byte payloads. Each data fragment is
  // dispatched once; duplicate final ACK recovery never produces ready twice.
  {
    std::vector<uint8_t> bytes(kRequest);for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<uint8_t>(i);
    Sender sender;Receiver receiver;uint8_t ack[kHeader];assert(sender.begin(30,bytes.data(),bytes.size()));
    uint32_t now=0;unsigned completed=0;
    while(sender.busy()) {
      size_t length=0;const uint8_t *frame=sender.poll(now,23,length);assert(frame&&length<=20);
      const auto result=receiver.accept(frame,length,now,ack);assert(result!=Receiver::Rejected);
      if(result==Receiver::Complete){++completed;assert(receiver.size()==bytes.size()&&!memcmp(bytes.data(),receiver.data(),bytes.size()));}
      assert(sender.acknowledge(ack,sizeof(ack)));now+=30;
    }
    assert(completed==1 && now>18000 && !sender.failed());
  }
  // Responses can reach65535bytes (not8192requests); maxMTU517 still caps GATT
  // at512. ACK for another ID/total/next-offset cannot release bytes.
  {
    std::vector<uint8_t> bytes(65535,0x81);Sender sender;assert(sender.begin(31,bytes.data(),bytes.size()));
    uint8_t ack[kHeader];size_t length=0;const auto *first=sender.poll(10,517,length);assert(first&&length==512);
    std::vector<uint8_t> original(first,first+length);
    header(ack,Ack,32,505,65535);assert(!sender.acknowledge(ack,sizeof(ack)));
    header(ack,Ack,31,504,65535);assert(!sender.acknowledge(ack,sizeof(ack)));
    assert(!sender.poll(709,23,length));
    const auto *retry=sender.poll(710,23,length);assert(retry&&length==512&&!memcmp(retry,original.data(),length));
    header(ack,Ack,31,505,65535);assert(sender.acknowledge(ack,sizeof(ack)));
    assert(!sender.acknowledge(ack,sizeof(ack)));
    const auto *next=sender.poll(711,23,length);assert(next&&length==20&&get16(next+3)==505);
    sender.reset();assert(!sender.busy());
    std::vector<uint8_t> tooLarge(65536);assert(!sender.begin(31,tooLarge.data(),tooLarge.size()));
  }
  // Bounded retry is of exact bytes only, never a new RPC/request or new ID.
  {
    const uint8_t bytes[]={1,2,3};Sender sender;assert(sender.begin(40,bytes,sizeof(bytes)));
    size_t length=0;uint32_t now=std::numeric_limits<uint32_t>::max()-200U;
    std::vector<uint8_t> original;
    for(unsigned attempt=0;attempt<kRetries;++attempt) {
      const auto *frame=sender.poll(now,23,length);assert(frame&&get16(frame+1)==40);
      if(!attempt)original.assign(frame,frame+length);else assert(!memcmp(original.data(),frame,length));
      now+=kRetryMs;
    }
    assert(!sender.poll(now,23,length)&&sender.failed()&&!sender.busy());
  }
  // Fragments alone are not authority: receipt consumes an intent BEFORE its
  // handler and conflicts on a changed path/form/session hash or deadline.
  {
    BoardCore::CommandReceipts receipts;BoardCore::CommandReceipts::Entry *slot=nullptr;
    const char *id="0123456789abcdef0123456789abcdef";
    const char *digest="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const char *changed="1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    assert(receipts.reserve(id,digest,5000,1000,slot)==BoardCore::CommandReceipts::Fresh);
    slot->accepted=true;assert(receipts.reserve(id,digest,5000,1100,slot)==BoardCore::CommandReceipts::Cached&&slot->accepted);
    assert(receipts.reserve(id,changed,5000,1100,slot)==BoardCore::CommandReceipts::Conflict);
    assert(receipts.reserve(id,digest,6000,1100,slot)==BoardCore::CommandReceipts::Conflict);
    assert(receipts.reserve(id,digest,10000,1000,slot)==BoardCore::CommandReceipts::Expired);
  }
  std::puts("BLE framing/ACK/ownership receipt source fixture passed");
}
