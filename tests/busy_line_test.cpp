#include "BusyLine.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
std::vector<uint8_t> calls;
void pinMode(uint8_t pin, uint8_t mode) { assert(pin == 0); calls.push_back(mode); }
void digitalWrite(uint8_t pin, uint8_t value) { assert(pin == 0 && value == LOW); calls.push_back(10); }
int main() {
  BusyLine<true> line(0); line.begin(); assert((calls == std::vector<uint8_t>{INPUT_PULLUP}));
  line.setBlocked(true); assert(calls.size() == 1);
  line.setBlocked(false); assert((calls == std::vector<uint8_t>{INPUT_PULLUP, 10, OUTPUT}));
  line.setBlocked(false); assert(calls.size() == 3);
  line.setBlocked(true); assert(calls.back() == INPUT_PULLUP);
  calls.clear(); BusyLine<false> polling(0); polling.begin(); polling.setBlocked(true); polling.setBlocked(false);
  assert(calls.empty());
  puts("Production busy line: explicit HIGH pull-up, LOW preload before output, no redundant transitions and no GPIO access in polling mode passed.");
}
