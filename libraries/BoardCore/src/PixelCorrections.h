#pragma once
#include <stdint.h>
#include <stddef.h>

namespace BoardCore {
// Addresses refer to the intermediate wire frame at this rule's position.
// Mapping is compiled only on configuration changes, never per animation frame.
template <size_t Pixels> class PixelCorrections {
 public:
  static_assert(Pixels > 0 && Pixels <= 480, "Correction mapping capacity");
  enum Kind : uint8_t { Black, Remove, Shift };
  struct Rule { uint16_t pixel = 0; int16_t offset = 0; Kind kind = Black; bool enabled = true; };
  static constexpr uint8_t maxRules = 32;
  PixelCorrections() { configure(false, nullptr, 0); }
  bool configure(bool enabled, const Rule *rules, size_t count) {
    if(count > maxRules || (count && !rules)) return false;
    for(size_t r=0;r<count;++r)
      if(rules[r].pixel >= Pixels || rules[r].kind > Shift ||
         rules[r].offset < -int(Pixels) || rules[r].offset > int(Pixels) ||
         (rules[r].kind != Shift && rules[r].offset != 0)) return false;
    for(size_t i=0;i<Pixels;++i) map_[i]=int16_t(i);
    if(!enabled) return true;
    for(size_t r=0;r<count;++r) {
      const auto &rule=rules[r];
      if(!rule.enabled)continue;
      if(rule.kind==Black) { map_[rule.pixel]=-1; continue; }
      if(rule.kind==Remove) {
        for(size_t i=rule.pixel;i+1<Pixels;++i) map_[i]=map_[i+1];
        map_[Pixels-1]=-1; continue;
      }
      if(rule.offset>0) {
        for(int i=int(Pixels)-1;i>=int(rule.pixel);--i)
          map_[i]=i-rule.offset>=int(rule.pixel)?map_[i-rule.offset]:-1;
      } else if(rule.offset<0) {
        const int start=int(rule.pixel)+rule.offset;
        const int end=int(Pixels)+rule.offset;
        for(int i=start<0?0:start;i<end;++i) map_[i]=map_[i-rule.offset];
        for(int i=end<int(rule.pixel)?int(rule.pixel):end;i<int(Pixels);++i)map_[i]=-1;
      }
    }
    return true;
  }
  int16_t source(size_t destination) const { return destination<Pixels?map_[destination]:-1; }
  template <class Pixel> void render(const Pixel *raw, Pixel *wire, const Pixel &black) const {
    // Separate buffers are required: removals/negative shifts can overwrite inputs.
    for(size_t i=0;i<Pixels;++i) wire[i]=map_[i]<0?black:raw[map_[i]];
  }
 private:
  int16_t map_[Pixels]{};
};
}
