#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

class __FlashStringHelper;
#define pgm_read_byte(address) (*reinterpret_cast<const uint8_t *>(address))
constexpr uint8_t PIN_PC0=14,PIN_PC1=15,PIN_PC2=16,PIN_PC3=17,PIN_PC4=18,PIN_PC5=19;
constexpr uint8_t PIN_PD2=2,PIN_PD3=3,PIN_PD4=4,PIN_PD5=5,PIN_PD6=6,PIN_PD7=7;
constexpr uint8_t PIN_PB0=8,PIN_PB1=9,PIN_PB2=10,PIN_PB3=11,PIN_PB4=12,PIN_PB5=13;
constexpr uint8_t OUTPUT=1,INPUT=0,INPUT_PULLUP=2,HIGH=1,LOW=0;

// A byte/transaction-decoding GPIO double, not a physical electrical model.
// The absent-device fixture deliberately reads LOW on an un-biased input:
// the old releaseData implementation therefore cannot pass presence tests.
namespace display_mock {
inline uint8_t mode=0x0F, output=0x0A;
inline bool connected=true, active=false;
inline uint8_t byte=0,bits=0;
inline unsigned reads=0,failRead=0;
inline std::vector<std::vector<uint8_t>> transactions;
inline std::vector<uint8_t> transaction;
inline uint32_t now=0;
inline bool clock() { return (output & 0x20)!=0; }
inline bool data() { return (output & 0x10)!=0; }
inline void changed(uint8_t oldMode,uint8_t oldOutput) {
  assert((mode & 0x0F)==0x0F && (output & 0x0F)==0x0A);
  const bool wasClock=(oldOutput & 0x20)!=0;
  const bool wasData=(oldOutput & 0x10)!=0;
  if (wasClock && clock() && (oldMode & mode & 0x10)) {
    if (wasData && !data()) {
      assert(!active); active=true; bits=byte=0; transaction.clear();
    } else if (!wasData && data() && active) {
      assert(!transaction.empty()); transactions.push_back(transaction);
      active=false; bits=byte=0;
    }
  }
  if (!wasClock && clock() && active) {
    if (bits < 8) {
      assert(mode & 0x10); // The host owns every data bit.
      byte |= static_cast<uint8_t>((data()?1:0) << bits++);
    } else {
      assert(!(mode & 0x10)); // Never drive high against the device ACK.
      assert(data()); // Internal pull-up must bias an unplugged line HIGH.
      transaction.push_back(byte); bits=byte=0;
    }
  }
}
inline void set(bool direction,uint8_t value) {
  const auto oldMode=mode,oldOutput=output;
  (direction?mode:output)=value; changed(oldMode,oldOutput);
}
inline uint8_t input() {
  assert(clock() && !(mode & 0x10));
  ++reads;
  return connected && reads!=failRead ? 0 : static_cast<uint8_t>(output & 0x10);
}
inline void reset(bool present=true) {
  mode=0x0F; output=0x0A; connected=present; active=false; byte=bits=0;
  reads=failRead=0; now=0; transactions.clear(); transaction.clear();
}
}
inline void pinMode(uint8_t pin,uint8_t value) {
  assert(pin==18 || pin==19); const auto mask=static_cast<uint8_t>(1U<<(pin-14));
  if (value==OUTPUT) display_mock::set(true,display_mock::mode|mask);
  else {
    display_mock::set(true,display_mock::mode & ~mask);
    display_mock::set(false,value==INPUT_PULLUP?display_mock::output|mask:display_mock::output & ~mask);
  }
}
inline void digitalWrite(uint8_t pin,uint8_t value) {
  assert(pin==18 || pin==19); const auto mask=static_cast<uint8_t>(1U<<(pin-14));
  display_mock::set(false,value?display_mock::output|mask:display_mock::output & ~mask);
}
inline int digitalRead(uint8_t pin) { assert(pin==18); return (display_mock::input() & 0x10)?HIGH:LOW; }
inline uint32_t millis() { return display_mock::now; }
inline void delayMicroseconds(unsigned) {}
