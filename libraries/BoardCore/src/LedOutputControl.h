#pragma once
#include <stddef.h>
#include <stdint.h>

namespace BoardCore {
// Living USB opcode 0x5c: preserve [enabled0/1]; append explicit mode-only
// [2, mode0static/1effects]. Neither operation changes the other setting.
struct LedOutputControl {bool modeOnly=false,value=false;uint8_t backend=0;};
inline bool parseLedOutputControl(const uint8_t *payload,size_t bytes,LedOutputControl &out){
  if(!payload)return false;
  if(bytes==1 && payload[0]<=1){out={false,payload[0]!=0};return true;}
  if(bytes==2 && payload[0]==2 && payload[1]<=1){out={true,payload[1]!=0};return true;}
  // Additive backend-only request: 1 GPIO, 2 RMT/DMA. Output enable and renderer
  // mode are independent; 0 is not an implicit Off or an unavailable fallback.
  if(bytes==2 && payload[0]==3 && payload[1]>=1 && payload[1]<=2){out={false,false,payload[1]};return true;}
  return false;
}
}
