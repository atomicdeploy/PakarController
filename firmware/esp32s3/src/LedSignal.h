#pragma once
#include <stddef.h>
#include <stdint.h>

namespace PakarS3 {
// Digital pad observation, NOT an analogue/DIN voltage or optical receipt.
struct LedSignalReport {
  uint32_t bits=0,expectedBits=0,timingErrors=0,firstMismatch=UINT32_MAX;
  uint32_t minHighNs=UINT32_MAX,maxHighNs=0,maxDataLowNs=0;
  bool match=false;
};
class LedSignalInspector {
 public:
  void reset(const uint8_t* expected,size_t bytes){out={};out.expectedBits=bytes*8U;expected_=expected;highSeen=expectLow=false;}
  template<class Symbol>
  void feed(const Symbol* symbols,size_t count) {
  for(size_t i=0;i<count;++i)for(unsigned half=0;half<2;++half) {
    const uint32_t ticks=half?symbols[i].duration1:symbols[i].duration0;
    const bool high=half?symbols[i].level1:symbols[i].level0;
    if(!ticks)continue; // Hardware EOF/idle marker is not a data bit.
    const uint32_t ns=ticks*25U; // Reviewed common 40MHz TX/RX clock.
    if(!high) {
      if(expectLow && out.bits<out.expectedBits && ns<100000U) {
        if(ns>out.maxDataLowNs)out.maxDataLowNs=ns;
        // One tick tolerance for digital sampling of the datasheet580..1000ns.
        if(ns<555U || ns>1025U)++out.timingErrors;
      }
      expectLow=false;continue; // Ignore initial LOW and final reset/latch LOW.
    }
    if(highSeen && expectLow)++out.timingErrors;
    highSeen=expectLow=true;
    if(ns<out.minHighNs)out.minHighNs=ns;
    if(ns>out.maxHighNs)out.maxHighNs=ns;
    const bool one=ns>=500U;
    if(one?(ns<555U||ns>1025U):(ns<195U||ns>405U))++out.timingErrors;
    const uint32_t bit=out.bits++;
    if(out.firstMismatch==UINT32_MAX && (bit>=out.expectedBits ||
        one!=bool(expected_[bit/8]&(0x80U>>(bit%8)))))out.firstMismatch=bit;
  }
  }
  LedSignalReport report()const {
  auto out=this->out;
  if(out.bits<out.expectedBits && out.firstMismatch==UINT32_MAX)out.firstMismatch=out.bits;
  out.match=out.bits==out.expectedBits && out.firstMismatch==UINT32_MAX && !out.timingErrors;
  return out;
  }
 private:
  LedSignalReport out;const uint8_t* expected_=nullptr;
  bool highSeen=false,expectLow=false;
};
template<class Symbol>
LedSignalReport inspectLedSignal(const Symbol* symbols,size_t count,
    const uint8_t* expected,size_t bytes) {
  LedSignalInspector inspector;inspector.reset(expected,bytes);inspector.feed(symbols,count);
  return inspector.report();
}
}
