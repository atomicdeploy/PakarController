#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace BoardCore {
// Same consume-before-dispatch and no-unexpired-eviction rule as CloudCommand
// Guard. RAM-only receipts make WS -> HTTP recovery safe for the SAME intent.
class CommandReceipts {
 public:
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  static constexpr size_t kCapacity = 64;
#else
  static constexpr size_t kCapacity = 16;
#endif
  enum Result { Fresh, Cached, Invalid, Expired, Conflict, Full };
  enum Failure : uint8_t { NoFailure, RedCellsLocked, InvalidCellAssignments, UnsupportedBoardRole, AvrOutcomeUnknown, AvrRejected };
  struct Entry {
    char id[33]{};char digest[65]{};uint64_t until=0;
    uint32_t observedRevision=0,observedLease=0;
    bool accepted=false;uint8_t count=0;Failure failure=NoFailure;
    void observe(uint32_t revision,uint32_t lease){observedRevision=revision;observedLease=lease;}
    bool observationMatches(uint32_t revision,uint32_t lease)const{return observedRevision==revision&&observedLease==lease;}
  };
  static const char *failureCode(Failure value) {
    return value==RedCellsLocked?"red_cells_locked":value==InvalidCellAssignments?"invalid_cell_assignments":
      value==UnsupportedBoardRole?"unsupported_board_role":value==AvrOutcomeUnknown?"avr_outcome_unknown":value==AvrRejected?"avr_command_rejected":"command_rejected";
  }
  static const char *failureMessage(Failure value) {
    return value==RedCellsLocked?"Red cells cannot become white while the red-cell lock is enabled.":
      value==InvalidCellAssignments?"Assignments must contain 1 to 80 unique cells and white, red or off states.":
      value==UnsupportedBoardRole?"This command is not supported by the board's baked-in role.":
      value==AvrOutcomeUnknown?"AVR confirmation was not received; do not replay this command.":
      value==AvrRejected?"The AVR rejected this command.":"The board rejected this command.";
  }
  // Two immutable lineage words add 512 bytes for 64 S3 slots. No payload or
  // transport budget grows; superseded leases cannot masquerade as old ACKs.
  static_assert(sizeof(Entry)<=128,"Command receipt RAM budget changed");
  Result reserve(const char *id,const char *digest,uint64_t until,uint64_t now,Entry *&slot) {
    slot=nullptr;
    if(!hex(id,32)||!hex(digest,64))return Invalid;
    if(until<=now || until-now>8000)return Expired;
    Entry *free=nullptr;
    for(auto &entry:entries_) {
      if(entry.id[0] && entry.until>now) {
        if(!strcmp(entry.id,id)) {
          if(strcmp(entry.digest,digest)||entry.until!=until)return Conflict;
          slot=&entry;return Cached;
        }
      } else if(!free)free=&entry;
    }
    if(!free)return Full;
    *free=Entry{};memcpy(free->id,id,33);memcpy(free->digest,digest,65);free->until=until;
    slot=free;return Fresh; // Reserved before any handler can change hardware.
  }
 private:
  static bool hex(const char *text,size_t size) {
    if(!text || strlen(text)!=size)return false;
    for(size_t i=0;i<size;++i)if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return false;
    return true;
  }
  Entry entries_[kCapacity]{};
};
}
