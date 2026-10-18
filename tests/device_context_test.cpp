#include <cassert>
#include <iostream>
#include "DeviceContext.h"
int main() {
  using namespace BoardCore::Context;
  using namespace BoardCore::Protocol;
  const uint16_t caps = kCapShiftInputs | kCapShiftOutputs | kCapTm1637 | kCapDisplayText | kCapBuzzer;
  assert(!displayObservationFresh(false,caps,true,0,10));
  assert(!displayObservationFresh(true,0,true,0,10));
  assert(!displayObservationFresh(true,caps,false,0,10));
  assert(displayObservationFresh(true,caps,true,0,2999));
  assert(!displayObservationFresh(true,caps,true,0,3000));
  assert(displayObservationFresh(true,caps,true,UINT32_MAX-1000U,1000));
  assert(strcmp(name(15),"lcd")==0);
  const auto offline = features(false, caps, true, true, true);
  assert(!(offline & (Display | Text | Buzzer | Inputs | Outputs | Settings)));
  const auto helloOnly = features(true, caps, false, false, true);
  assert((helloOnly & Buzzer) && !(helloOnly & (Display | Text)));
  assert(!(features(true, kCapTm1637, true, true, false) & (Text | Buzzer)));
  assert((features(true, kCapTm1637, true, true, false) & Display));
  assert(!(features(true, kCapDisplayText, true, true, false) & Text));
  const auto live = features(true, caps, true, true, true);
  assert((live & (Display | Text | Buzzer)) == (Display | Text | Buzzer));
  assert(!(features(true, caps, true, false, true) & (Display | Text)));
  assert(!(features(true, 0, true, true, true) & (Display | Text | Buzzer)));
  if (PAKAR_TABLE_ENABLED) {
    assert((live & Table)); assert((live & CellBatch));
    assert((modes(live) & 8) == (PAKAR_SNAKE_ENABLED ? 8 : 0));
    assert(strcmp(name(14),"cell-batch")==0);
    assert((modes(live) & 4) == (PAKAR_FIRST_WINS_ENABLED ? 4 : 0));
    assert(!(modes(offline) & 4));
    assert(!(modes(live)&1)); // No Idle presentation; output has its own service switch.
  } else { assert(!(live & Table)); assert(modes(live) == 0); }
  assert(modes(Display | Buzzer) == 0);
  std::cout << "Peer context: individual HELLO, live display ACK, stale/missing evidence and Table-only modes passed\n";
}
