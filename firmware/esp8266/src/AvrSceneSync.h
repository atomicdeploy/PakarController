#pragma once
#include <stdint.h>
// Reacquisition is a baseline, not a physical Reset/key action. Setting the
// paired mode itself increments AVR resetRevision, so baseline its reply too.
class AvrSceneSync {
 public:
  void hello() { initial_ = true; waiting_ = false; }
  struct Result { bool baseline, sendMode; };
  Result observe(uint8_t observedMode, uint8_t desiredMode) {
    if (initial_) {
      initial_ = false; waiting_ = observedMode != desiredMode;
      return {true, waiting_};
    }
    if (waiting_) {
      if (observedMode == desiredMode) waiting_ = false;
      return {true, false}; // Never replay an uncertain SetMode.
    }
    return {false, false};
  }
 private:
  bool initial_ = true, waiting_ = false;
};
