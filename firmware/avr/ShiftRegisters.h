#pragma once

#include <Arduino.h>

class ShiftRegisters {
 public:
  void begin();
  void service();
  uint8_t activeInputs() const { return static_cast<uint8_t>(~input_); }
  uint8_t activeOutputs() const { return static_cast<uint8_t>(~output_); }
  void setActiveOutputs(uint8_t mask) { output_ = static_cast<uint8_t>(~mask); }
  void allOutputsOff() { output_ = 0xFF; }

 private:
  uint8_t input_ = 0xFF;
  uint8_t output_ = 0xFF;
};

extern ShiftRegisters shiftRegisters;
