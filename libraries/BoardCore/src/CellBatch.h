#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace BoardCore {
// One bounded selection, not80 client requests. Validate into a temporary before
// touching renderer state, following AddressableLeds::stagePixels/show ownership.
struct CellBatch {
  static constexpr uint8_t cells=80,bytes=10;
  uint8_t mask[bytes]{};
  bool contains(uint8_t id)const{return id>=1&&id<=cells&&(mask[(id-1)/8]&(1U<<((id-1)%8)));}
  uint8_t count()const{uint8_t n=0;for(uint8_t id=1;id<=cells;++id)if(contains(id))++n;return n;}
  uint8_t first()const{for(uint8_t id=1;id<=cells;++id)if(contains(id))return id;return 0;}
  void add(uint8_t id){if(id>=1&&id<=cells)mask[(id-1)/8]|=1U<<((id-1)%8);}
  static bool parse(const char *text,size_t length,CellBatch &out){
    if(!text||!length||length>512)return false;
    CellBatch next;size_t at=0;
    const auto spaces=[&](){while(at<length&&(text[at]==' '||text[at]=='\t'))++at;};
    const auto number=[&](uint8_t &value){
      spaces();unsigned n=0;const size_t start=at;
      while(at<length&&text[at]>='0'&&text[at]<='9'){
        n=n*10U+unsigned(text[at++]-'0');if(n>cells)return false;
      }
      if(at==start||!n)return false;
      value=static_cast<uint8_t>(n);spaces();return true;
    };
    while(at<length){
      uint8_t start=0,end=0;if(!number(start))return false;end=start;
      if(at<length&&text[at]=='-'){++at;if(!number(end)||end<start)return false;}
      for(uint8_t id=start;id<=end;++id)next.add(id);
      if(at==length)break;
      if(text[at++]!=','||at==length)return false;
    }
    if(!next.count())return false;
    out=next;return true;
  }
  static bool decode(const uint8_t *payload,uint8_t length,CellBatch &out){
    if(!payload||length!=12||payload[0]>2||payload[1]>2)return false;
    CellBatch next;memcpy(next.mask,payload+2,bytes);
    if(!next.count())return false;
    out=next;return true;
  }
};
}
