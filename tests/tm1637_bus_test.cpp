#include <Arduino.h>
#include "SevenSegments.h"
#include "BoardPins.h"
#include <iostream>
#include <stdexcept>

// Actual PCController Tools/VirtualBoard/tests/firmware_controls_test.cpp
// require/testDisplayBrightnessFade/testDisplayAckDetectionAndRecovery adapted
// to Pakar's separate health service and confirmed A4/A5 pins. The other donor
// contracts remain in the ledger, not certified by these display tests.
namespace {
void require(bool condition,const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}
void testDisplayBrightnessFade() {
  display_mock::reset();
  SevenSegments segments; segments.begin(5);
  segments.serviceBrightness(0,69);
  require(segments.brightness()==5,"brightness moved before the fade interval");
  for (uint32_t tick=70;tick<=350;tick+=70) segments.serviceBrightness(0,tick);
  require(segments.brightness()==0,"fade did not reach display-off");
  require(display_mock::transactions.back()==std::vector<uint8_t>{0x80},"zero did not send real display-off");
  for (uint32_t tick=420;tick<=910;tick+=70) segments.serviceBrightness(9,tick);
  require(segments.brightness()==7,"brightness did not clamp/reach full level");
  require(display_mock::transactions.back()==std::vector<uint8_t>{0x8F},"full-brightness command mismatch");
}
void testDisplayAckDetectionAndRecovery() {
  display_mock::reset();
  SevenSegments segments;
  require(!segments.detected(),"presence inferred before any ACK");
  segments.begin(5); segments.showText("ACK ");
  require(segments.detected(),"ACK was not recognized");
  require(display_mock::transactions.back()==std::vector<uint8_t>({0xC0,0x77,0x39,0x76,0}),"address/frame did not match actual text");
  const auto count=display_mock::transactions.size();
  segments.showText("ACK ");
  require(display_mock::transactions.size()==count,"unchanged cells retransmitted before keepalive");
  display_mock::connected=false;
  segments.service(999); require(segments.detected(),"health check ran too early");
  segments.service(1000); require(!segments.detected(),"unplugged floating bus became phantom ACK");
  display_mock::connected=true;
  const auto start=display_mock::transactions.size(); segments.service(2000);
  require(segments.detected(),"ACK recovery failed");
  require(display_mock::transactions[start]==std::vector<uint8_t>{0x40} &&
          display_mock::transactions[start+1]==std::vector<uint8_t>({0xC0,0x77,0x39,0x76,0}) &&
          display_mock::transactions[start+2]==std::vector<uint8_t>{0x8D},"recovery did not replay mode/frame/brightness");
  for (unsigned byte=1;byte<=7;++byte) {
    display_mock::failRead=display_mock::reads+byte;
    segments.service(2000+byte*1000);
    require(!segments.detected(),"a later ACK hid an intermediate NACK");
  }
  display_mock::failRead=0; segments.service(10000);
  require(segments.detected(),"recovery from NACK failed");
  segments.setBrightness(0); display_mock::connected=false; segments.service(11000);
  require(!segments.detected(),"off state hid unplugging");
  display_mock::connected=true; segments.service(12000);
  require(segments.detected() && segments.brightness()==0 &&
          display_mock::transactions.back()==std::vector<uint8_t>{0x80},"recovery turned an intentionally off display on");
  segments.service(UINT32_MAX-500); const auto before=display_mock::transactions.size();
  segments.service(498); require(display_mock::transactions.size()==before,"rollover keepalive ran early");
  segments.service(499); require(display_mock::transactions.size()==before+3,"rollover keepalive missed deadline");
  require(!display_mock::active,"bus left in an unfinished transaction");
}
}
int main() {
  try {
    static_assert(BoardPins::Tm1637Data==PIN_PC4 && BoardPins::Tm1637Clock==PIN_PC5,"ISP pins must not be used by display");
    testDisplayBrightnessFade(); testDisplayAckDetectionAndRecovery();
    std::cout << "TM1637 bus: pulled-up ACK, decoded frames, NACK positions, off recovery, fades and rollover passed\n";
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
