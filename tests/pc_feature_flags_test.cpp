#include "PcControllerRuntime.h"
#include "PcLedContract.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
struct Hardware : PcControllerHardware {
  uint8_t response[48]={}; uint8_t size=0;
  Snapshot snapshot() const override { return {}; }
  BoardCore::Protocol::Error execute(uint8_t,const uint8_t*,uint8_t,uint32_t) override {
    assert(false); return BoardCore::Protocol::kUnsafe;
  }
  void applySettings(const BoardSettings&) override {}
  BoardCore::Protocol::Error menuAction(uint8_t,uint8_t) override { assert(false); return BoardCore::Protocol::kUnsafe; }
  void showMenu(uint8_t,bool,bool) override {}
  void safeStop() override {}
  bool ready() const override { return false; }
  bool send(uint8_t,uint8_t,const uint8_t *p,uint8_t n) override { size=n; memcpy(response,p,n);return true; }
};
struct Strip {}; // Disabled engine must not instantiate any pin/output methods.
int main() {
  static_assert(BoardCore::Features::enabled==0,"test needs disabled profile");
  Hardware h; PcControllerRuntime runtime(h); runtime.begin(0);
  uint8_t hello[]={ControllerProtocol::Hello}; runtime.handle(1,hello,1,1,1);
  assert(h.size==9 && h.response[5]==0 && h.response[6]==0);
  for(uint8_t op:{ControllerProtocol::MenuAction,ControllerProtocol::MacroStart,ControllerProtocol::AutomationList}) {
    uint8_t p[]={op};runtime.handle(1,p,1,1,1);
    assert(h.size==3 && h.response[0]==ControllerProtocol::ErrorResponse &&
      h.response[2]==BoardCore::Protocol::kUnsupported);
  }
  Strip strip; BoardCore::PcLedContract<Strip> led(strip);
  assert(led.handle(nullptr,0)==BoardCore::Protocol::kHardwareUnavailable);
  puts("Disabled menu/macro/automation/ESP LED profile: no hardware mutation or false capabilities passed.");
}
