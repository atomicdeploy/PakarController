#include <SerialLink.h>
#include "../firmware/esp/src/HostDemand.h"
#include <cassert>
#include <cstdio>
using namespace BoardCore::Protocol;
struct Port:HardwareSerial {
  unsigned calls=0,failAt=0xffffffffU;
  size_t write(const uint8_t *data,size_t bytes)override {
    if(calls++==failAt)return 0;
    return HardwareSerial::write(data,bytes);
  }
  int peek(){return received.empty()?-1:received.front();}
  void flush(){}
  int availableForWrite(){return 256;}
};
static void capture(const Frame &frame,void *context){static_cast<std::vector<Frame>*>(context)->push_back(frame);}
static std::vector<uint8_t> encoded(uint8_t opcode,uint8_t sequence) {
  Frame frame{};frame.opcode=opcode;frame.sequence=sequence;
  uint8_t bytes[kMaximumEncoded];const auto count=encode(frame,bytes,sizeof(bytes));assert(count);
  return {bytes,bytes+count};
}
int main() {
  HostDemand demand;
  assert(demand.flags(0)==0&&!demand.wants(UsbDemand::Pixels,0));
  assert(!demand.configure(0x10,1000,0)&&!demand.configure(1,249,0));
  assert(!demand.configure(1,5001,0)&&!demand.configure(0,1000,0));
  assert(demand.configure(UsbDemand::Status|UsbDemand::Pixels,3000,100));
  assert(demand.remaining(100)==3000&&demand.wants(UsbDemand::Pixels,3099));
  assert(demand.flags(3100)==0);
  assert(demand.configure(UsbDemand::Panel,250,0xffffff80U));
  assert(demand.remaining(0x79U)==1&&demand.flags(0x7aU)==0);
  assert(demand.configure(UsbDemand::Peripheral,5000,0));
  assert(demand.configure(0,0,1)&&!demand.flags(1));
  assert(demand.configure(UsbDemand::Status,3000,0));
  assert(!demand.configure(0x8000,3000,1)&&!demand.flags(1));
  Port port;HostSerialAdapter<Port> adapter(port,demand);
  BoardCore::SerialLink link(static_cast<Stream&>(adapter));
  link.begin(115200,nullptr); // Does not reconfigure the selected transport.
  assert(port.baud==0);
  const uint8_t payload[4]={3,0,0xb8,0x0b};
  assert(demand.configure(UsbDemand::Panel,3000,0));
  assert(link.send(kHostDemandResponse,23,payload,4));
  assert(demand.wants(UsbDemand::Panel,1));
  Frame decoded{};
  assert(decode(port.written.data(),port.written.size()-1,decoded));
  assert(decoded.opcode==kHostDemandResponse&&decoded.sequence==23&&decoded.payloadLength==4);
  port.calls=0;port.failAt=1;assert(!link.ack(24,kGetStatus));
  assert(!demand.flags(2)); // Partial TX never leaves optional publishing armed.
  port.failAt=0xffffffffU;port.written.clear();
  assert(link.ack(25,kGetStatus)); // Explicit recovery reply does not need a lease.
  assert(port.written.front()==0); // Existing SerialLink resync, not replay.
  // An observed native detach retires ALL old-session RX, including a partial
  // parser packet followed by queued old HELLO/mutation bytes. Reattach must
  // accept a fresh complete HELLO without requiring a leading zero delimiter.
  std::vector<Frame> received;
  link.begin(115200,capture,&received);
  const auto oldHello=encoded(kHello,30),oldMutation=encoded(kGameReset,31);
  const size_t split=oldHello.size()/2;
  port.feed({oldHello.begin(),oldHello.begin()+split});link.service();
  assert(received.empty());
  port.feed({oldHello.begin()+split,oldHello.end()});port.feed(oldMutation);
  demand.clear();link.discardInput();link.begin(115200,capture,&received);
  assert(port.received.empty()&&port.baud==0);
  port.feed(encoded(kHello,41));link.service();
  assert(received.size()==1&&received.front().opcode==kHello&&received.front().sequence==41);
  puts("USB demand: default quiet, finite lease/rollover/release, invalid fail-closed, real COBS response and short-write retirement passed.");
}
