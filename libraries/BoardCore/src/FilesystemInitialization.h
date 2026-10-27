#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace BoardCore {
// A mount failure is NOT evidence of an empty partition. This guard accepts
// only a complete erased-byte scan, and consumes confirmation before a write.
class FilesystemInitialization {
 public:
  void clear(){token_=0;size_=checked_=0;ready_=false;}
  bool begin(uint32_t size,bool mounted,bool idle) {
    clear();if(!size||mounted||!idle)return false;size_=size;return true;
  }
  bool inspect(uint32_t offset,const uint8_t *data,size_t count) {
    if(!size_||ready_||offset!=checked_||!data||!count||count>size_-checked_){clear();return false;}
    for(size_t i=0;i<count;++i)if(data[i]!=0xff){clear();return false;}
    checked_+=count;return true;
  }
  bool arm(uint32_t token,uint32_t now) {
    if(!token||!size_||checked_!=size_||ready_)return false;
    token_=token;armed_=now;ready_=true;return true;
  }
  bool consume(uint32_t token,uint32_t now,bool mounted,bool idle,const char *confirmation) {
    const bool accepted=ready_&&token&&token==token_&&uint32_t(now-armed_)<30000U&&
        !mounted&&idle&&confirmation&&!strcmp(confirmation,"INITIALIZE EMPTY FILESYSTEM");
    clear();return accepted;
  }
 private:
  uint32_t token_=0,size_=0,checked_=0,armed_=0;
  bool ready_=false;
};
}
