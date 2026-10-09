#pragma once

#include <Arduino.h>

// Tiny checksum-backed EEPROM record owned by PakarController. The latch is
// intentionally independent of game state: if a flash/update is interrupted,
// the AVR restarts with outputs off, audio muted, and "Prog" visible.
class ProgrammingLatch {
 public:
  void begin();
  bool active() const { return active_; }
  bool set(bool active);

 private:
  bool active_ = false;
};

extern ProgrammingLatch programmingLatch;
