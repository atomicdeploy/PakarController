#pragma once
#include "ShellWire.h"

namespace PakarShell {
struct PixelCommand {
  uint16_t index=0;
  uint8_t count=1;
  uint32_t rgb=0xffffff,revision=0;
  bool off=false;
};
// Numeric indices remain API zero-based. Named groups are physical, one-based,
// never logical cell IDs. Resolve both through the exact live topology used by
// the API and TUI, including custom-length groups and the used pixel prefix.
template<class Layout> bool pixelCommand(const char* args,const Layout& layout,PixelCommand& result){
  result={};result.revision=layout.revision();
  if(!args)return false;
  char words[3][17]{};size_t count=0;
  while(*args){
    while(*args==' ')++args;
    if(!*args)break;
    if(count==3)return false;
    size_t n=0;
    while(*args&&*args!=' '){if(n==16)return false;words[count][n++]=*args++;}
    ++count;
  }
  if(count==1&&!strcmp(words[0],"off")){result.off=true;return true;}
  if(!count)return false;
  uint32_t position=0,length=1;
  if(!strcmp(words[0],"group")){
    if(count<2||!unsignedNumber(words[1],Layout::groupCount,position)||!position)return false;
    result.index=layout.start(position);result.count=layout.count(position);
  }else{
    if(!layout.used()||!unsignedNumber(words[0],layout.used()-1,position))return false;
    if(count>=2&&(!unsignedNumber(words[1],255,length)||!length))return false;
    result.index=position;result.count=length;
  }
  if(count==3){
    if(strlen(words[2])!=6)return false;
    result.rgb=0;
    for(const char* p=words[2];*p;++p){
      const int digit=*p>='0'&&*p<='9'?*p-'0':*p>='a'&&*p<='f'?*p-'a'+10:*p>='A'&&*p<='F'?*p-'A'+10:-1;
      if(digit<0)return false;
      result.rgb=(result.rgb<<4)|digit;
    }
  }
  return layout.diagnosticRange(result.index,result.count);
}
}
