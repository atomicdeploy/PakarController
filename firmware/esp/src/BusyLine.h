#pragma once
#include <Arduino.h>
// Active-high, fail-quiet flow control. HIGH releases the line with an explicit
// internal pull-up; LOW alone is driven. GPIO0 must remain HIGH during boot.
template <bool Enabled> class BusyLine {
 public:
  explicit BusyLine(uint8_t pin) : pin_(pin) {}
  void begin() { initialized_ = false; setBlocked(true); }
  void setBlocked(bool blocked) {
    if (!Enabled || (initialized_ && blocked_ == blocked)) return;
    if (blocked) pinMode(pin_, INPUT_PULLUP);
    else {
      digitalWrite(pin_, LOW); // preload LOW before enabling the output driver
      pinMode(pin_, OUTPUT);
    }
    initialized_ = true; blocked_ = blocked;
  }
 private:
  uint8_t pin_;
  bool initialized_ = false;
  bool blocked_ = true;
};
