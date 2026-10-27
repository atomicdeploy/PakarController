#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace BoardCore {
// Physical cable topology, independent of logical names/permutations. Zero in
// the override record inherits the baked default; every effective group exists.
template <size_t Groups,uint8_t DefaultCount,size_t Capacity> class PixelGroupLayout {
 public:
  static_assert(Groups>0&&Groups<=255&&DefaultCount>0&&Groups*DefaultCount<=Capacity&&Capacity<=65535,"Invalid pixel topology capacity");
  static constexpr size_t groupCount=Groups,capacity=Capacity;
  static constexpr uint8_t defaultCount=DefaultCount;
  PixelGroupLayout(){uint8_t defaults[Groups]{};configure(defaults,0);}
  bool configure(const uint8_t *overrides,uint32_t revision) {
    if(!overrides)return false;
    uint32_t total=0;
    for(size_t i=0;i<Groups;++i)total+=overrides[i]?overrides[i]:DefaultCount;
    if(total>Capacity)return false;
    uint16_t start=0;
    for(size_t i=0;i<Groups;++i){overrides_[i]=overrides[i];starts_[i]=start;start+=count(i+1);}
    used_=start;revision_=revision;return true;
  }
  uint8_t count(uint8_t group)const{return validGroup(group)?(overrides_[group-1]?overrides_[group-1]:DefaultCount):0;}
  uint16_t start(uint8_t group)const{return validGroup(group)?starts_[group-1]:used_;}
  uint8_t overrideCount(uint8_t group)const{return validGroup(group)?overrides_[group-1]:0;}
  uint16_t used()const{return used_;}
  uint32_t revision()const{return revision_;}
  bool customized()const{for(auto value:overrides_)if(value)return true;return false;}
  bool sameOverrides(const PixelGroupLayout &other)const{return !memcmp(overrides_,other.overrides_,Groups);}
  uint8_t groupAt(uint16_t pixel)const {
    if(pixel>=used_)return 0;
    for(size_t i=0;i<Groups;++i)if(pixel<starts_[i]+count(i+1))return i+1;
    return 0;
  }
  bool diagnosticRange(uint16_t pixel,uint8_t length)const {
    const uint8_t group=groupAt(pixel);
    return group && (length==1 || (pixel==start(group)&&length==count(group)));
  }
 private:
  static bool validGroup(uint8_t group){return group>=1&&group<=Groups;}
  uint8_t overrides_[Groups]{};
  uint16_t starts_[Groups]{},used_=0;
  uint32_t revision_=0;
};
}
