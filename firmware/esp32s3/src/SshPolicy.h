#pragma once
#include <stdint.h>
#include <stddef.h>

namespace PakarS3::SshPolicy {
constexpr uint32_t handshakeMs=15000,loginMs=30000,idleMs=1800000,drainMs=30000;
constexpr size_t packetBytes=128;
constexpr unsigned inputBudget=64;
inline bool portAllowed(uint16_t port,uint16_t telnetPort){
  return port&&port!=telnetPort&&port!=80&&port!=443&&port!=7777;
}
inline bool expired(uint32_t now,uint32_t since,uint32_t lease){return uint32_t(now-since)>=lease;}
// Queue tokens are never recycled during a boot. Stale bytes cannot reach a new
// editor after reconnect, auth changes or a listener reconfiguration.
inline bool admitted(uint32_t token,uint32_t active,uint32_t revision,uint32_t current){
  return token&&token==active&&revision&&revision==current;
}
inline uint16_t columns(int value){return value<40?40:value>240?240:uint16_t(value);}
inline uint16_t rows(int value){return value<16?16:value>100?100:uint16_t(value);}
// Exactly one bounded device command, not a script or operating-system shell.
// Reject controls before they can become a second editor submission.
inline size_t commandLength(const char* text){
  if(!text)return 0;
  bool content=false;size_t n=0;
  for(;n<=packetBytes&&text[n];++n){
    const auto ch=static_cast<unsigned char>(text[n]);
    if(ch<32||ch>126)return 0;
    content|=ch!=' ';
  }
  return content&&n<=packetBytes?n:0;
}
}
