#include "../firmware/esp8266/src/UrbootProtocol.h"
#include <cassert>
#include <deque>
#include <vector>
#include <array>
#include <cstdio>
struct Port {
 uint32_t clock=0;std::deque<uint8_t>rx;std::vector<uint8_t>request;
 std::array<uint8_t,32768>flash{};std::array<uint8_t,1024>eeprom{};
 uint8_t in=160,ok=119;bool shortWrite=false,badReply=false,silent=false;
 unsigned writes=0,erases=0,eeWrites=0;
 Port(){for(unsigned i=0;i<flash.size();++i)flash[i]=i*17;
  const uint8_t meta[]={3,25,0xcd,0xcf,0xe7,0x40};
  for(unsigned i=0;i<6;++i)flash[32762+i]=meta[i];}
 int available(){return int(rx.size());}int read(){if(rx.empty())return -1;auto v=rx.front();rx.pop_front();return v;}
 uint32_t now(){return clock;}void idle(){++clock;}
 size_t write(const uint8_t *data,size_t n){
  if(shortWrite)return n-1;
  request.insert(request.end(),data,data+n);
  if(request.empty())return n;
  const uint8_t command=request[0];
  const unsigned expected=command==2?133:command==1||command==3?5:2;
  if(request.size()<expected)return n;
  assert(request.size()==expected && request.back()==0x20);
  if(command==0x52)++erases;
  if(command==0)++eeWrites;
  if(!silent) {
   rx.push_back(badReply?uint8_t(in^1):in);
   if(command==1||command==3){const unsigned at=request[1]+256U*request[2];
    for(unsigned i=0;i<request[3];++i)rx.push_back(command==1?eeprom.at(at+i):flash.at(at+i));}
   if(command==2){++writes;const unsigned at=request[1]+256U*request[2];
    for(unsigned i=0;i<128;++i)flash.at(at+i)=request[4+i];}
   rx.push_back(ok);
  }
  request.clear();return n;
 }
};
int main(){
 {Port p;PakarUrboot::Protocol<Port>u(p);assert(u.connect());
  uint8_t data[128];assert(u.read(false,32760,data,8));
  for(unsigned i=0;i<128;++i)data[i]=i^0x5a;
  assert(u.program(128,data)&&p.writes==1);
  uint8_t back[128];assert(u.read(false,128,back,128));
  for(unsigned i=0;i<128;++i)assert(back[i]==data[i]);
  assert(u.read(true,0,back,128));assert(!p.erases&&!p.eeWrites);u.leave();}
 for(unsigned at:{1U,32384U,32768U}){Port p;PakarUrboot::Protocol<Port>u(p);assert(u.connect());
  uint8_t page[128]={};assert(!u.program(at,page));assert(!p.writes);}
 {Port p;p.ok=120;PakarUrboot::Protocol<Port>u(p);assert(!u.connect()&&!p.writes);}
 {Port p;p.flash[32762]=4;PakarUrboot::Protocol<Port>u(p);assert(!u.connect()&&!p.writes);}
 for(uint8_t features:{uint8_t(0xc7),uint8_t(0xef),uint8_t(0xf7),uint8_t(0xff)}){
  Port p;p.flash[32766]=features;PakarUrboot::Protocol<Port>u(p);assert(!u.connect()&&!p.writes);}
 {Port p;p.silent=true;p.clock=UINT32_MAX-20;PakarUrboot::Protocol<Port>u(p);assert(!u.connect());assert(uint32_t(p.clock-(UINT32_MAX-20))<=480);}
 {Port p;PakarUrboot::Protocol<Port>u(p);assert(u.connect());p.shortWrite=true;uint8_t page[128]={};
  assert(!u.program(0,page)&&p.writes==0);}
 {Port p;PakarUrboot::Protocol<Port>u(p);assert(u.connect());p.badReply=true;uint8_t page[128]={};
  assert(!u.program(0,page)&&p.writes==1);assert(!u.program(0,page)&&p.writes==1);}
 puts("Actual Urboot protocol: MCU/profile/byte-address framing, flash/EEPROM reads, protected full-page write, timeout/rollover, no erase/EEPROM-write/write replay passed (UART emulator, not hardware).");
}
