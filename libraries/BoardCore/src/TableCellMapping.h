#pragma once
#include <stdint.h>

namespace BoardCore { namespace TableCellMapping {
// Six consecutive wire pixels form one cell. The cable snakes through eight
// cells per lane, starting at logical80,60,40,20,10,30,50,70. The final lane
// is71,51,31,11,1,21,41,61; logical1 occupies physical group77.
constexpr uint8_t logicalForPhysical(uint8_t physical) {
  if (physical < 1 || physical > 80) return 0;
  constexpr uint8_t firstLane[] = {80,60,40,20,10,30,50,70};
  return firstLane[(physical-1)%8] - (physical-1)/8;
}
constexpr uint8_t physicalForLogical(uint8_t logical) {
  if (logical < 1 || logical > 80) return 0;
  for (uint8_t physical=1;physical<=80;++physical)
    if (logicalForPhysical(physical)==logical) return physical;
  return 0;
}
} }
