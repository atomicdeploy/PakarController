#include "RadioAdapter.h"
#include <RadioReception.h>
#include <assert.h>
#include <stdio.h>
struct Sink {
  unsigned count=0;
  void radioReceived(uint32_t code,uint8_t bits,uint8_t protocol,uint16_t pulse){
    assert(code==123 && bits==24 && protocol==1 && pulse==350);++count;
  }
};
int main(){
  using namespace BoardCore::Protocol;
  RadioAdapter radio;Sink sink;radio.begin(3,0);
  assert(RCSwitch::tx==3 && RCSwitch::rx==0 && RCSwitch::repeats==3);
  RCSwitch::code=123;radio.service(sink);assert(sink.count==1 && !RCSwitch::code);
  radio.service(sink);assert(sink.count==1);
  RCSwitch::code=123;radio.service(sink);assert(sink.count==2 && !RCSwitch::code);
  RCSwitch::code=123;RCSwitch::bits=33;radio.service(sink);assert(sink.count==2);
  BoardCore::RadioReception received;
  uint8_t event[]={ControllerProtocol::Event,8,0x78,0x56,0x34,0x12,32,13,0x5e,1,0xff};
  assert(received.decode(event,sizeof(event)));
  assert(received.code==0x12345678 && received.bits==32 && received.protocol==13 && received.pulseMicros==350);
  assert(!received.decode(event,10));event[6]=33;assert(!received.decode(event,sizeof(event)));
  uint8_t payload[]={123,0,0,0,24,1,0,0};
  assert(radio.transmit(payload,8,true)==kBusy && !RCSwitch::sends && !RCSwitch::disables);
  assert(radio.transmit(payload,7,false)==kBadPayload);
  payload[5]=13;assert(radio.transmit(payload,8,false)==kBadPayload);payload[5]=1;
  payload[6]=1;assert(radio.transmit(payload,8,false)==kBadPayload);payload[6]=0;
  assert(radio.transmit(payload,8,false)==kNoError);
  assert(RCSwitch::sends==1 && RCSwitch::disables==1 && RCSwitch::rx==0);
  puts("RF adapter: raw repeat forwarding, ESP wire decoding, validation, armed-round TX rejection and RX restoration passed (mock radio).");
}
