#include "StatusRgb.h"
#include <cassert>
#include <cstdio>
#include <vector>

using Engine = PakarS3::StatusRgb;
struct Frame { int pin; uint8_t red, green, blue; };
int main() {
  Engine engine;
  std::vector<Frame> frames;
  bool writable = true;
  auto writer = [&](int pin, uint8_t red, uint8_t green, uint8_t blue) {
    if (!writable) return false;
    frames.push_back({pin, red, green, blue}); return true;
  };
  assert(!engine.snapshot().available && !engine.snapshot().haveOutput);
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
  engine.service(900, writer);
  assert(engine.snapshot().red > 0 && engine.snapshot().green == 0);
  const auto peak = engine.snapshot().red;
  engine.service(1400, writer);
  assert(engine.snapshot().red < peak); // No slow-time catch-up/counter engine.
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
  puts("S3 status RGB: explicit pin safety, separate matrix, semantic transitions, authoritative output, deduplication, driver failure, disable and wrap pass; not optical acceptance.");
}
