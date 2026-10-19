#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace PakarNbns {
// Adapted from the bundled Arduino3.3.12 NetBIOS and ESP8266NetBIOS A..P
// first-level encoding/answer layout. RFC1002 sections4.2.12-18 supply strict
// questions and complete NBSTAT statistics. No file-server/session names.
constexpr size_t kQuestionBytes=50,kMaximumReply=121;
inline uint16_t get16(const uint8_t* p){return uint16_t(p[0])<<8|p[1];}
inline void put16(uint8_t* p,uint16_t n){p[0]=uint8_t(n>>8);p[1]=uint8_t(n);}
inline void put32(uint8_t* p,uint32_t n){p[0]=uint8_t(n>>24);p[1]=uint8_t(n>>16);p[2]=uint8_t(n>>8);p[3]=uint8_t(n);}
inline bool hostname(const char* input,char (&name)[16]){
  memset(name,0,sizeof(name));if(!input||!input[0])return false;
  size_t n=0;
  while(input[n]){
    if(n==15){memset(name,0,sizeof(name));return false;}
    const char c=input[n];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')){memset(name,0,sizeof(name));return false;}
    name[n]=c>='a'&&c<='z'?char(c-'a'+'A'):c;++n;
  }
  if(name[0]=='-'||name[n-1]=='-'){memset(name,0,sizeof(name));return false;}
  return true;
}
inline size_t answer(const uint8_t* question,size_t length,const char* name,
    uint32_t address,const uint8_t (&mac)[6],uint8_t* output,size_t capacity){
  if(!question||length!=kQuestionBytes||!name||!name[0]||!output||!address)return 0;
  const uint16_t flags=get16(question+2),type=get16(question+46);
  if((flags&uint16_t(~0x0110U))||get16(question+4)!=1||get16(question+6)||get16(question+8)||get16(question+10)||
      question[12]!=32||question[45]!=0||get16(question+48)!=1||(type!=0x20&&type!=0x21))return 0;
  uint8_t decoded[16];
  for(unsigned i=0;i<16;++i){
    const uint8_t a=question[13+i*2],b=question[14+i*2];
    if(a<'A'||a>'P'||b<'A'||b>'P')return 0;
    decoded[i]=uint8_t((a-'A')*16+(b-'A'));
  }
  // Only <00> workstation identity; do not advertise an SMB <20> service.
  if(decoded[15]!=0)return 0;
  bool wildcard=decoded[0]=='*',matches=true;size_t n=0;
  while(n<16&&name[n])++n;
  if(n>15)return 0;
  for(unsigned i=0;i<15;++i){
    uint8_t c=decoded[i];if(c>='a'&&c<='z')c=uint8_t(c-'a'+'A');
    if(c!=(i<n?uint8_t(name[i]):uint8_t(' ')))matches=false;
    if(i&&decoded[i]!=0&&decoded[i]!=' ')wildcard=false;
  }
  if(!matches&&!(type==0x21&&wildcard))return 0;
  const size_t size=type==0x20?62:kMaximumReply;
  if(capacity<size)return 0;
  memset(output,0,size);memcpy(output,question,2);
  put16(output+2,uint16_t(0x8400U|(type==0x20?(flags&0x0100U):0U)));
  put16(output+6,1);memcpy(output+12,question+12,34);
  put16(output+46,type);put16(output+48,1);
  if(type==0x20){
    put32(output+50,300);put16(output+54,6);put32(output+58,address);
  }else{
    put16(output+54,65);output[56]=1;memset(output+57,' ',15);memcpy(output+57,name,n);
    output[72]=0;put16(output+73,0x0400); // One unique, active B-node workstation.
    memcpy(output+75,mac,6); // Complete46-byte statistics; no sessions/SMB claimed.
  }
  return size;
}
} // namespace PakarNbns
