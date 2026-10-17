#pragma once
#include <Arduino.h>

// Adapted from PCController Project/ResetTelemetry.{h,cpp}: early hardware reset
// cause plus MCU-owned, CRC/commit-last, wear-levelled boot count. Pakar owns a
// separate EEPROM region; this service never touches settings or the Prog latch.
class ResetTelemetry {
 public:
  void begin();
  uint8_t cause()const{return cause_;}
  uint32_t count()const{return count_;}
  bool persisted()const{return persisted_;}
  bool allowWelcome()const{
    // ATmega328P MCUSR: PORF=1, EXTRF=2, BORF=4, WDRF=8. Repeated
    // watchdog/brownout resets and an unknown cause must not repeat a melody.
    return (cause_&3U)!=0 && (cause_&12U)==0;
  }
 private:
  uint32_t count_=0;
  uint8_t cause_=0;
  bool persisted_=false,begun_=false;
};
extern ResetTelemetry resetTelemetry;
