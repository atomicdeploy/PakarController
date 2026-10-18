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
  struct Entry { char id[33]{}; char digest[65]{}; uint64_t until=0; bool accepted=false; uint8_t count=0; };
  static_assert(sizeof(Entry)<=120,"Command receipt RAM budget changed");
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
