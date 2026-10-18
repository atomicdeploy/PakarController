#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace PakarShell {
// Additive COBS channel. Raw terminal bytes never share the binary decoder.
// Open is boot-fenced by its caller; every later packet carries a fresh token.
class SerialChannel {
 public:
  enum Action : uint8_t { Open=0, Input=1, Close=2, Poll=3 };
  enum Result : uint8_t { Accepted=0, Duplicate=1, Bad=2, Stale=3, Sequence=4 };
  void open(uint32_t token) { token_=token;next_=0;lastLength_=0;haveLast_=false; }
  void close() { token_=0;next_=0;haveLast_=false;memset(last_,0,sizeof(last_)); }
  bool active()const{return token_!=0;}
  uint32_t token()const{return token_;}
  uint16_t next()const{return next_;}
  Result accept(uint32_t token,uint16_t sequence,const uint8_t *bytes,size_t size) {
    if(!active()||token!=token_)return Stale;
    if(!bytes||!size||size>41)return Bad;
    if(haveLast_ && sequence==uint16_t(next_-1))
      return size==lastLength_&&!memcmp(last_,bytes,size)?Duplicate:Sequence;
    if(next_==UINT16_MAX)return Sequence; // Reopen before wrap; no old sequence can become new input.
    if(sequence!=next_)return Sequence;
    memcpy(last_,bytes,size);lastLength_=uint8_t(size);haveLast_=true;++next_;
    return Accepted;
  }
 private:
  uint32_t token_=0;uint16_t next_=0;uint8_t last_[41]{},lastLength_=0;bool haveLast_=false;
};
class OutputWindow {
 public:
  bool acknowledge(uint16_t ack,size_t &consumed){
    consumed=0;
    if(size_&&ack==sequence_){consumed=size_;size_=0;++sequence_;return true;}
    return ack==uint16_t(sequence_-1);
  }
  bool stage(const uint8_t *bytes,size_t size){
    if(size_||!size)return true;
    // UINT16_MAX is the initial no-output ACK sentinel. Never emit it or
    // reuse a sequence in this session; a busy TUI must reopen before wrap.
    if(sequence_==UINT16_MAX)return false;
    size_=size>38?38:size;memcpy(bytes_,bytes,size_);return true;
  }
  uint16_t sequence()const{return sequence_;}
  const uint8_t *bytes()const{return bytes_;}
  uint8_t size()const{return size_;}
 private:uint16_t sequence_=0;uint8_t bytes_[38]{},size_=0;
};
inline bool unsignedNumber(const char *text,uint32_t maximum,uint32_t &value) {
  if(!text||!*text)return false;
  uint32_t result=0;
  for(const char *p=text;*p;++p){
    if(*p<'0'||*p>'9')return false;
    const uint32_t digit=*p-'0';
    if(digit>maximum || result>(maximum-digit)/10U)return false;
    result=result*10U+digit;
  }
  value=result;return true;
}
} // namespace PakarShell
