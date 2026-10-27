#pragma once
#include <stdint.h>
namespace BoardCore { namespace FlashToneSequence {
// Shared finite presentation clock. No heap, per-note transport or AVR ISR.
inline bool valid(uint16_t on,uint16_t off,uint16_t last,uint8_t flashes) {
  return on>=10&&on<=5000&&off<=5000&&last>=10&&last<=5000&&flashes&&flashes<=32;
}
inline uint32_t duration(uint16_t on,uint16_t off,uint16_t last,uint8_t flashes) {
  return uint32_t(flashes-1)*(uint32_t(on)+off)+last+off;
}
inline bool sample(uint16_t on,uint16_t off,uint16_t last,uint8_t flashes,
                   uint32_t elapsed,uint16_t &hz,uint16_t &remaining,uint32_t &began) {
  const uint32_t finalStart=uint32_t(flashes-1)*(uint32_t(on)+off);
  if(elapsed>=duration(on,off,last,flashes))return false;
  const uint8_t pulse=elapsed>=finalStart?flashes-1:elapsed/(uint32_t(on)+off);
  const uint32_t start=uint32_t(pulse)*(uint32_t(on)+off);
  const uint16_t lit=pulse==flashes-1?last:on;
  const uint32_t phase=elapsed-start;
  // C-major ascent resolves on a higher C; the final note follows the last
  // long flash exactly. Silence follows the actual dark interval, not RTT.
  const uint8_t note=pulse%4;
  hz=phase<lit?(note==0?1047:note==1?1319:note==2?1568:2093):0;
  began=start+(phase<lit?0:lit);
  remaining=phase<lit?lit-phase:uint32_t(lit)+off-phase;
  return true;
}
}}
