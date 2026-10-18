#include "EspFrontPanel.h"
#include "PcControllerRuntime.h"
#include <EEPROM.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
namespace P = BoardCore::Protocol;
namespace E = P::EspPanel;
struct Settings final : EspFrontPanel::Settings {
  E::Snapshot actual;
  bool readable = true, persist = true, lie = false;
  unsigned mutations = 0, scans = 0, reconnects = 0;
  Settings() { actual.mode = 2; actual.stationConfigured = true; strcpy(actual.hostname,"ledboard"); }
  bool read(E::Snapshot &out) const override { out = actual; return readable; }
  P::Error setMode(uint8_t mode) override {
    ++mutations; if (!persist) return P::kHardwareUnavailable;
    if (!lie) actual.mode = mode;
    return P::kNoError;
  }
  P::Error setDhcp(bool enabled) override {
    ++mutations; if (!persist) return P::kHardwareUnavailable;
    if (!lie) actual.dhcp = enabled;
    return P::kNoError;
  }
  P::Error reconnect() override { ++reconnects; return P::kNoError; }
  P::Error scan() override { ++scans; return P::kNoError; }
};
struct Sender {
  P::Frame sent{};
  bool send(uint8_t op, uint8_t seq, const uint8_t *p, uint8_t n) {
    sent.opcode = op; sent.sequence = seq; sent.payloadLength = n;
    memcpy(sent.payload,p,n); return true;
  }
};
struct Hardware final : PcControllerHardware {
  Snapshot live{};
  Sender transport;
  bool active = false, ready_ = true;
  uint8_t page = 0, notice = 0, value = 0;
  unsigned cues = 0;
  Snapshot snapshot() const override { return live; }
  P::Error execute(uint8_t,const uint8_t *,uint8_t,uint32_t) override { return P::kNoError; }
  void applySettings(const BoardSettings &) override {}
  P::Error menuAction(uint8_t,uint8_t) override { return P::kNoError; }
  void showMenu(uint8_t,bool,bool) override {}
  void showEspMenu(bool a,uint8_t p,uint8_t n,uint8_t v,const char *,uint32_t) override {
    active=a;page=p;notice=n;value=v;
  }
  void menuNavigationCue(bool) override { ++cues; }
  void safeStop() override {}
  bool ready() const override { return ready_; }
  bool send(uint8_t op,uint8_t seq,const uint8_t *p,uint8_t n) override { return transport.send(op,seq,p,n); }
};
P::Frame request(uint8_t action,uint8_t value=0) {
  P::Frame f{}; f.opcode=P::kEspPanel; f.sequence=7;
  f.payloadLength=action==E::SetMode||action==E::SetDhcp?2:1;
  f.payload[0]=action; f.payload[1]=value; return f;
}
int main() {
  Settings settings; Sender wire; EspFrontPanel service;
  assert(service.offer(settings,wire,0));
  assert(wire.sent.opcode==P::kEspPanel && wire.sent.payload[0]==E::Offer);
  E::Snapshot decoded;
  assert(E::decodeSnapshot(wire.sent.payload+1,wire.sent.payloadLength-1,decoded));
  assert(decoded.mode==2 && !strcmp(decoded.hostname,"ledboard"));
  auto f=request(E::SetMode,3);
  settings.persist=false;
  assert(service.handle(f,100,settings,wire));
  assert(wire.sent.payload[1]==P::kHardwareUnavailable && settings.actual.mode==2);
  const auto writes=settings.mutations;
  assert(service.handle(f,101,settings,wire) && settings.mutations==writes);
  settings.persist=true; f.sequence=8;
  service.handle(f,102,settings,wire);
  assert(wire.sent.payload[1]==0 && settings.actual.mode==3);
  settings.lie=true; f=request(E::SetDhcp,0); f.sequence=9;
  service.handle(f,103,settings,wire);
  assert(wire.sent.payload[1]==P::kHardwareUnavailable && settings.actual.dhcp);
  settings.lie=false; settings.actual.stationConfigured=false;
  f=request(E::SetMode,2); f.sequence=10; service.handle(f,104,settings,wire);
  assert(wire.sent.payload[1]==P::kUnsafe);
  f=request(E::Reconnect); f.sequence=11; service.handle(f,105,settings,wire);
  assert(wire.sent.payload[1]==P::kUnsafe && !settings.reconnects);
  f=request(E::Scan); f.sequence=12; service.handle(f,106,settings,wire);
  assert(!wire.sent.payload[1] && settings.scans==1);
  service.handle(f,107,settings,wire); assert(settings.scans==1);
  // Query must observe live network changes even with an identical sequence;
  // only mutating actions reuse the prior admitted outcome.
  f=request(E::Query);f.sequence=13;settings.actual.stationConnected=false;
  service.handle(f,108,settings,wire);
  assert(E::decodeSnapshot(wire.sent.payload+2,wire.sent.payloadLength-2,decoded)&&!decoded.stationConnected);
  settings.actual.stationConnected=true;service.handle(f,109,settings,wire);
  assert(E::decodeSnapshot(wire.sent.payload+2,wire.sent.payloadLength-2,decoded)&&decoded.stationConnected);
  f=request(E::Scan);f.sequence=12;service.handle(f,110,settings,wire);assert(settings.scans==1);
  settings.actual.stationConfigured=true; settings.actual.mode=2;
  EEPROM.reset(); Hardware hardware; PcControllerRuntime avr(hardware); avr.begin(0);
  avr.key(8,KeyEvent::HoldStart,0,1,1000);
  assert(!avr.espMenuActive()); // No invented capability/state.
  service.offer(settings,wire,0);
  assert(avr.observeEspPanel(wire.sent,100));
  avr.key(8,KeyEvent::HoldStart,0,110,110000);
  assert(avr.espMenuActive() && hardware.active && hardware.value==2 && hardware.cues==1);
  avr.key(3,KeyEvent::Down,0,120,120000);
  assert(hardware.value==3 && hardware.notice==E::Draft && settings.actual.mode==2);
  avr.key(8,KeyEvent::Click,0,130,130000);
  assert(hardware.notice==E::Pending && hardware.transport.sent.payload[0]==E::SetMode);
  assert(settings.actual.mode==2); // No optimistic local state mutation.
  auto action=hardware.transport.sent;
  service.handle(action,131,settings,wire);
  const auto good=wire.sent;
  ++wire.sent.sequence;
  assert(!avr.observeEspPanel(wire.sent,132)); // Wrong sequence cannot ACK.
  assert(avr.observeEspPanel(good,133));
  assert(hardware.notice==E::Saved && settings.actual.mode==3);
  avr.key(1,KeyEvent::Down,0,140,140000); // Reconnect page.
  avr.key(8,KeyEvent::Click,0,150,150000);
  assert(hardware.notice==E::Pending);
  avr.service(1650,1650000); assert(hardware.notice==E::TimedOut);
  service.handle(hardware.transport.sent,1651,settings,wire);
  assert(!avr.observeEspPanel(wire.sent,1652)); // Late success cannot replace timeout.
  const uint8_t localPage=avr.menuPage();
  avr.key(8,KeyEvent::DoubleClick,0,1700,1700000);
  assert(!avr.espMenuActive() && avr.menuPage()==localPage);
  // Actual refresh is required after expiry; an old cached offer isn't live.
  avr.service(6000,6000000);
  avr.key(8,KeyEvent::HoldStart,0,6001,6001000);
  assert(!avr.espMenuActive());
  service.offer(settings,wire,0); avr.observeEspPanel(wire.sent,6010);
  hardware.live.programming=true;
  avr.key(8,KeyEvent::HoldStart,0,6011,6011000);
  assert(!avr.espMenuActive());
  puts("Paired ESP front panel: actual offer, physical keys, drafts, persistence/readback, replay, correlation, timeout, stale and Prog gates pass; not physical acceptance.");
}
