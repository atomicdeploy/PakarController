#include "StatusRgb.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using Engine = PakarS3::StatusRgb;
struct Frame { int pin; uint8_t red, green, blue; };
int main() {
  Engine::Observations observed;
  assert(Engine::selectState(observed)==Engine::State::LocalReady);
  observed.networkReady=true;
  assert(Engine::selectState(observed)==Engine::State::Ready); // AVR-disabled is not peer-lost.
  observed.peerRequired=true;
  assert(Engine::selectState(observed)==Engine::State::PeerLost);
  observed.peerOnline=true;observed.networkReady=false;observed.accessPoint=true;
  assert(Engine::selectState(observed)==Engine::State::AccessPoint);
  observed.stationEnabled=true;
  assert(Engine::selectState(observed)==Engine::State::Connecting);
  observed.booting=true;
  assert(Engine::selectState(observed)==Engine::State::Boot);
  observed.updating=true;
  assert(Engine::selectState(observed)==Engine::State::Updating);
  observed.fault=true;
  assert(Engine::selectState(observed)==Engine::State::Fault);
  observed.fault=false;observed.shutdown=true;
  assert(Engine::selectState(observed)==Engine::State::Shutdown);
  observed={};observed.networkReady=true;observed.accessPoint=true;
  assert(Engine::selectState(observed)==Engine::State::AccessPointNetwork);
  assert(!strcmp(Engine::name(Engine::selectState(observed)),"access-point-network")); // Ethernet need not imply a station link.
  observed.success=true;
  assert(Engine::selectState(observed)==Engine::State::Success);
  observed.peerRequired=true;
  assert(Engine::selectState(observed)==Engine::State::PeerLost); // No success masks a lost required AVR.
  observed.warning=true;
  assert(Engine::selectState(observed)==Engine::State::Warning);
  Engine engine;
  std::vector<Frame> frames;
  bool writable = true;
  auto writer = [&](int pin, uint8_t red, uint8_t green, uint8_t blue) {
    if (!writable) return false;
    frames.push_back({pin, red, green, blue}); return true;
  };
  assert(!engine.snapshot().available && !engine.snapshot().haveOutput);
  Engine initiallyOff;Engine::Configuration off;
  off.pin=48;off.pinConfirmed=true;off.enabled=false;
  assert(initiallyOff.configure(off,0,writer));
  assert(frames.size()==1&&frames.back().red==0&&frames.back().green==0&&frames.back().blue==0);
  assert(initiallyOff.snapshot().haveOutput); // Confirmed Off actually submitted black, even on first configuration.
  frames.clear();
  assert(!engine.service(0, writer) && frames.empty());
  Engine::Configuration config;
  config.pin = 48; config.enabled = true;
  assert(!engine.configure(config, 0, writer)); // No guessed onboard pin.
  assert(frames.empty());
  config.pinConfirmed = true; config.pin = 4;
  assert(!engine.configure(config, 0, writer)); // Never drive matrix via status.
  config.pin = 17;
  assert(!engine.configure(config, 0, writer)); // Never drive AVR TX.
  config.pin = 48;
  assert(engine.configure(config, 0, writer));
  assert(engine.snapshot().available && engine.snapshot().haveOutput);
  engine.setState(Engine::State::Ready, 0);
  engine.service(0, writer);
  engine.service(150, writer);
  assert(engine.snapshot().green > 0 && engine.snapshot().green < 64);
  engine.service(300, writer);
  assert(engine.snapshot().red == 0 && engine.snapshot().green == 64);
  assert(engine.snapshot().brightness == 64); // Already-scaled output bytes.
  const auto revision = engine.snapshot().revision;
  engine.setState(Engine::State::Ready, 350);
  assert(!engine.service(350, writer));
  assert(engine.snapshot().revision == revision); // No duplicate state events.
  engine.setState(Engine::State::Fault, 400);
  engine.service(400, writer);
  assert(engine.snapshot().green == 64); // New mode cross-fades from actual frame.
  engine.service(700, writer);
  assert(engine.snapshot().red == 64 && engine.snapshot().green == 0);
  assert(engine.snapshot().effect == static_cast<uint8_t>(Engine::Effect::DoubleBlink));
  engine.service(900, writer);
  assert(engine.snapshot().red == 0); // Real black between fault blink pairs.
  engine.service(1600, writer);
  assert(engine.snapshot().red == 64); // Late service jumps to current period, never catches up slowly.
  engine.setState(Engine::State::Updating, 2000);
  engine.setProgress(100);
  engine.service(2300, writer);
  assert(engine.snapshot().progress == 100 && engine.snapshot().blue == 64);
  engine.setProgress(200);
  engine.service(2316, writer);
  assert(engine.snapshot().progress == 255);
  writable = false;
  const auto beforeFailure = engine.snapshot().revision;
  engine.setState(Engine::State::Warning, 2500);
  assert(!engine.service(2600, writer));
  assert(engine.snapshot().revision == beforeFailure); // Failed driver not ACKed.
  config.enabled = false;
  assert(!engine.configure(config, 2700, writer)); // Cannot pretend extinguished.
  writable = true;
  assert(engine.configure(config, 2700, writer));
  assert(!engine.snapshot().enabled && engine.snapshot().red == 0 &&
         engine.snapshot().green == 0 && engine.snapshot().blue == 0);
  assert(frames.back().red == 0 && frames.back().green == 0 && frames.back().blue == 0);
  config.pin = 38; config.enabled = true;
  assert(engine.configure(config, 2800, writer));
  assert(frames.back().pin == 38);
  // millis wrap: phase arithmetic stays elapsed-based, not a signed deadline.
  engine.setState(Engine::State::AccessPoint, UINT32_MAX - 200U);
  engine.service(100, writer);
  assert(engine.snapshot().condition == Engine::State::AccessPoint);
  assert(engine.snapshot().updatedAt == 100);
  engine.setState(Engine::State::Shutdown,1000);
  engine.service(1350,writer);
  assert(engine.snapshot().blue > 0 && engine.snapshot().blue < 64);
  engine.service(1700,writer);
  assert(engine.snapshot().red==0 && engine.snapshot().green==0 && engine.snapshot().blue==0);
  const auto blackRevision=engine.snapshot().revision;
  assert(!engine.service(2400,writer) && engine.snapshot().revision==blackRevision); // No repeating shutdown fade.
  engine.setState(Engine::State::Warning,2500);
  engine.service(2800,writer);assert(engine.snapshot().red==64);
  engine.service(3000,writer);assert(engine.snapshot().red==0);
  engine.setState(Engine::State::Connecting,3500);
  engine.service(3950,writer);const auto pulseFloor=engine.snapshot().red;
  engine.service(4900,writer);assert(engine.snapshot().red>pulseFloor);
  assert(engine.snapshot().effect==static_cast<uint8_t>(Engine::Effect::Pulse));
  engine.setState(Engine::State::AccessPointNetwork,5000);
  engine.service(8200,writer);const auto startBlue=engine.snapshot().blue;
  engine.service(9800,writer);assert(engine.snapshot().blue>startBlue);
  assert(engine.snapshot().effect==static_cast<uint8_t>(Engine::Effect::Cycle));
  writable=false;engine.setState(Engine::State::Fault,10000);
  assert(!engine.service(10000,writer));
  writable=true;engine.setProgress(1);
  assert(!engine.service(10016,writer)); // Metadata changes cannot hammer a failing RMT driver.
  assert(engine.service(10250,writer));
  for(uint8_t effect=0;effect<=6;++effect)assert(Engine::effectName(effect)[0]!='u');
  assert(Engine::effectName(255)[0]=='u');
  puts("S3 status RGB: safe pins, static/breathe/pulse/blink/double-blink/color-fade/shutdown, state priority, offline USB, progress, wrap, dedup, bounded failure recovery and real black pass; not optical acceptance.");
}
