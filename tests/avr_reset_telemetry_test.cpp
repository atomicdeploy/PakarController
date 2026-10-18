// Adapt actual PCController Tools/VirtualBoard/tests/reset_telemetry_test.cpp:
// same torn/CRC/slot/counter rollover semantics, with Pakar ownership bounds.
#include "../firmware/avr/ResetTelemetry.h"
#include <EEPROM.h>
#include <WireCodec.h>
#include <pccontroller/EepromLayout.h>
#include <cassert>
#include <cstdio>
#include <cstring>
void pakarTestSetResetCause(uint8_t);
int address(uint8_t slot){return EepromLayout::ResetJournalAddress+slot*6;}
uint8_t crc(uint32_t count){return BoardCore::WireCodec::crc8(reinterpret_cast<uint8_t*>(&count),4);}
void record(uint8_t slot,uint32_t count,bool committed=true,bool corrupt=false){
  const int at=address(slot);memcpy(EEPROM.bytes+at,&count,4);
  EEPROM.bytes[at+4]=crc(count)^(corrupt?0x5a:0);EEPROM.bytes[at+5]=committed?0xa7:0;
}
bool valid(uint8_t slot,uint32_t expected){
  uint32_t count;memcpy(&count,EEPROM.bytes+address(slot),4);
  return count==expected && EEPROM.bytes[address(slot)+4]==crc(count) && EEPROM.bytes[address(slot)+5]==0xa7;
}
int main(){
  static_assert(EepromLayout::ResetJournalAddress==416 && EepromLayout::ResetJournalEnd==800,"Pakar reset ownership moved");
  EEPROM.reset();memset(EEPROM.bytes,0x5a,416);memset(EEPROM.bytes+800,0x5a,224);
  pakarTestSetResetCause(1);ResetTelemetry first;first.begin();
  assert(first.count()==1 && first.cause()==1 && first.persisted() && first.allowWelcome() && valid(0,1));
  for(unsigned i=0;i<1024;++i)if(i<416 || i>=800)assert(EEPROM.bytes[i]==0x5a);
  const auto writes=EEPROM.writes;first.begin();assert(first.count()==1 && EEPROM.writes==writes);
  // A Prog-held reset is observable but cannot invalidate an ISP EEPROM backup.
  EEPROM.reset();record(5,41);pakarTestSetResetCause(2);
  uint8_t snapshot[1024];memcpy(snapshot,EEPROM.bytes,sizeof(snapshot));
  const auto beforeProg=EEPROM.writes;ResetTelemetry readOnly;readOnly.begin(false);
  assert(readOnly.count()==42 && readOnly.cause()==2 && !readOnly.persisted());
  assert(EEPROM.writes==beforeProg && memcmp(snapshot,EEPROM.bytes,sizeof(snapshot))==0);
  readOnly.begin();assert(!readOnly.persisted() && EEPROM.writes==beforeProg);
  assert(memcmp(snapshot,EEPROM.bytes,sizeof(snapshot))==0);
  EEPROM.reset();ResetTelemetry emptyReadOnly;emptyReadOnly.begin(false);
  assert(emptyReadOnly.count()==1 && !emptyReadOnly.persisted() && EEPROM.writes==0);
  EEPROM.reset();record(5,41);record(6,42,true,true);record(7,43,false);
  ResetTelemetry recovery;recovery.begin();assert(recovery.count()==42 && recovery.persisted() && valid(6,42));
  assert(EEPROM.bytes[address(7)+5]==0);
  for(int cut=0;cut<7;++cut){
    EEPROM.reset();record(5,41);EEPROM.budget=cut;ResetTelemetry interrupted;interrupted.begin();
    assert(interrupted.count()==42 && !interrupted.persisted());
    EEPROM.budget=-1;ResetTelemetry restart;restart.begin();assert(restart.count()==42 && restart.persisted() && valid(6,42));
  }
  EEPROM.reset();for(uint8_t slot=0;slot<64;++slot)record(slot,100U+slot);
  ResetTelemetry rollover;rollover.begin();assert(rollover.count()==164 && valid(0,164));
  EEPROM.reset();record(62,UINT32_MAX-1);
  ResetTelemetry maximum;maximum.begin();assert(maximum.count()==UINT32_MAX && valid(63,UINT32_MAX));
  ResetTelemetry wrap;wrap.begin();assert(wrap.count()==1 && valid(0,1));
  ResetTelemetry next;next.begin();assert(next.count()==2 && valid(1,2));
  for(uint8_t cause=0;cause<16;++cause){
    pakarTestSetResetCause(cause);ResetTelemetry observed;observed.begin();
    assert(observed.cause()==cause && observed.allowWelcome()==((cause&3U)!=0 && (cause&12U)==0));
  }
  puts("Actual AVR reset journal: ownership416..799, cause capture projection, idempotence, CRC/torn-write recovery,64-slot/count rollover, readback failure and quiet WDT/BOR/unknown welcome policy passed. Host EEPROM double only, not AVR startup/Urboot or physical reset proof.");
}
