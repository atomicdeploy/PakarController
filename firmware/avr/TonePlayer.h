#pragma once

#include <Arduino.h>

class TonePlayer {
 public:
  void begin(uint8_t pin);
  void clear();
  bool add(uint16_t frequency, uint16_t durationMs);
  void service(uint32_t now);
  bool busy() const;
  void welcome();
  void winner(uint8_t winner);
  void error();

 private:
  struct Note { uint16_t frequency; uint16_t durationMs; };
  static constexpr uint8_t kCapacity = 16;
  Note queue_[kCapacity] = {};
  uint8_t length_ = 0;
  uint8_t index_ = 0;
  uint8_t pin_ = 0;
  uint32_t finishesAt_ = 0;
};

extern TonePlayer tones;
