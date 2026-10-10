#pragma once
#include <assert.h>
#include <stdint.h>
#include <string.h>
class MockEeprom {
 public:
  uint8_t bytes[1024];
  int budget = -1;
  unsigned writes = 0;
  MockEeprom() { reset(); }
  void reset() { memset(bytes, 0xFF, sizeof(bytes)); budget = -1; writes = 0; }
  void update(int address, uint8_t value) {
    assert(address >= 0 && address < 1024);
    if (bytes[address] == value || budget == 0) return;
    if (budget > 0) --budget;
    bytes[address] = value; ++writes;
  }
  template<class T> void get(int address, T &value) const {
    assert(address >= 0 && address + sizeof(T) <= 1024);
    memcpy(&value, bytes + address, sizeof(T));
  }
};
inline MockEeprom EEPROM;
