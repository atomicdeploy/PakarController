#include "ResetTelemetry.h"
#include <EEPROM.h>
#include <WireCodec.h>
#include <pccontroller/EepromLayout.h>
#include <stddef.h>
#if defined(__AVR__)
#include <avr/io.h>
#include <avr/wdt.h>
#endif

// Actual PCController donorb0111ae0 Project/ResetTelemetry.cpp supplies capture,
// record validation, serial-number ordering and invalidate/publish-last journal.
// Adaptations: Pakar416..799 reservation, existing identical CRC-8/ATM helper,
// idempotent begin, final EEPROM readback and explicit welcome-reset policy.
namespace {
constexpr uint8_t kMarker=0xA7;
struct __attribute__((packed)) ResetRecord {
  uint32_t count;
  uint8_t checksum,marker;
};
static_assert(sizeof(ResetRecord)==EepromLayout::ResetRecordBytes,"Reset record changed");
static_assert(EepromLayout::ResetJournalEnd<=1008,"Reset journal overlaps Prog");
#if defined(__AVR__)
static_assert(EepromLayout::ResetJournalEnd<=E2END+1,"Reset journal exceeds AVR EEPROM");
uint8_t capturedResetCause __attribute__((section(".noinit")));
void capturePakarResetCause() __attribute__((naked,used,section(".init3")));
void capturePakarResetCause(){
  uint8_t cause=MCUSR;
  if(cause==0){
    // Urboot preserves the original MCUSR in r2 before clearing it.
    register uint8_t urbootCause asm("r2");
    cause=urbootCause;
  }
  capturedResetCause=cause;
  MCUSR=0;
  wdt_disable();
  // Naked .init3 deliberately falls through to the remaining C startup.
  // No EEPROM, melody, Arduino clock or C++ object access is allowed here.
}
#else
uint8_t capturedResetCause=0;
#endif
uint8_t checksum(uint32_t count){
  return BoardCore::WireCodec::crc8(reinterpret_cast<const uint8_t*>(&count),sizeof(count));
}
bool valid(const ResetRecord &record){
  return record.marker==kMarker && record.count && record.checksum==checksum(record.count);
}
bool newer(uint32_t candidate,uint32_t reference){return static_cast<int32_t>(candidate-reference)>0;}
int slotAddress(uint8_t slot){return EepromLayout::ResetJournalAddress+static_cast<int>(slot)*sizeof(ResetRecord);}
}

#if defined(PAKAR_RESET_NATIVE_TEST)
void pakarTestSetResetCause(uint8_t cause){capturedResetCause=cause;}
#endif
ResetTelemetry resetTelemetry;
void ResetTelemetry::begin(bool allowWrite){
  if(begun_)return;
  begun_=true;cause_=capturedResetCause;
  bool found=false;uint8_t newest=0;ResetRecord record;
  for(uint8_t slot=0;slot<EepromLayout::ResetJournalSlots;++slot){
    EEPROM.get(slotAddress(slot),record);
    if(valid(record)&&(!found||newer(record.count,count_))){found=true;newest=slot;count_=record.count;}
  }
  count_=count_==UINT32_MAX?1:count_+1;
  // Still expose this boot's cause/count, but leave the ISP backup byte-exact.
  // begin is idempotent: a later default call must not turn this into a write.
  if(!allowWrite)return;
  const uint8_t next=found?static_cast<uint8_t>((newest+1)%EepromLayout::ResetJournalSlots):0;
  const int address=slotAddress(next);
  EEPROM.update(address+offsetof(ResetRecord,marker),0);
  const uint8_t *bytes=reinterpret_cast<const uint8_t*>(&count_);
  for(uint8_t i=0;i<sizeof(count_);++i)EEPROM.update(address+i,bytes[i]);
  EEPROM.update(address+offsetof(ResetRecord,checksum),checksum(count_));
  EEPROM.update(address+offsetof(ResetRecord,marker),kMarker);
  EEPROM.get(address,record);persisted_=valid(record)&&record.count==count_;
}
