#pragma once
#include <stdint.h>
namespace PakarLed {
// Same loop-owned renderer and capacity-one output mailbox, never a second
// effects task/engine. Cloud IO calls this only between complete bounded writes;
// individual SDK socket calls may still block and remain a separate metric.
template<class Grid>
bool serviceDuringCloud(Grid &grid,uint32_t now,bool transactionBusy,bool visualActive,bool uartReady) {
  if(transactionBusy||visualActive||!uartReady||!grid.needsRender(now))return false;
  grid.service(now);return true;
}
}
