// Executes production FrontPanel.cpp against a display transport double.
#include "FrontPanel.h"
#include "SevenSegments.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

SevenSegments display;
std::string shown;
std::array<uint8_t, 4> raw{};
bool isRaw = false;
void SevenSegments::commit(const uint8_t cells[4]) {
  std::copy(cells, cells + 4, raw.begin()); isRaw = true;
}
void SevenSegments::showText(const char *text) {
  shown.clear();
  for (unsigned i = 0; i < 4 && text[i]; ++i) shown += text[i];
  isRaw = false;
}
void SevenSegments::showText(const __FlashStringHelper *text) {
  showText(reinterpret_cast<const char *>(text));
}
void SevenSegments::showInteger(int32_t value) { shown = std::to_string(value); isRaw = false; }

std::vector<uint8_t> message(const char *text, uint8_t flags = 0,
                             uint16_t hold = 160, uint8_t interval = 1) {
  std::vector<uint8_t> p = {5, 80, 0, static_cast<uint8_t>(strlen(text)), flags,
                          static_cast<uint8_t>(hold), static_cast<uint8_t>(hold >> 8), interval};
  p.insert(p.end(), text, text + strlen(text)); return p;
}
void tick(FrontPanel &panel, uint32_t now, bool programming = false) {
  panel.service(now, BoardCore::Protocol::kModeCellSelect, 1, 0, programming);
}
void accept(FrontPanel &panel, const std::vector<uint8_t> &p, uint32_t now) {
  assert(panel.acceptDisplayPayload(p.data(), static_cast<uint8_t>(p.size()), now));
}

int main() {
  // Long text scrolls once, including one blank frame, then releases locally.
  FrontPanel once; accept(once, message("HELLO"), 1000);
  tick(once, 1000); assert(shown == "HELL");
  tick(once, 1400); // missed ticks advance one frame, never skip the terminal blank
  for (uint32_t now = 1480; now <= 1720; now += 80) tick(once, now);
  assert(shown == "    " && once.hostActive());
  tick(once, 1800); assert(!once.hostActive() && shown == "PAKr");
  tick(once, 9000); assert(!once.hostActive());

  FrontPanel legacy; accept(legacy, {0,80,0,5,'H','E','L','L','O'}, 1000);
  for (uint32_t now = 1000; now <= 1480; now += 80) tick(legacy, now);
  assert(!legacy.hostActive());

  FrontPanel loop; accept(loop, message("HELLO", 1), 1000);
  for (uint32_t now = 1000; now <= 1480; now += 80) tick(loop, now);
  assert(loop.hostActive() && shown == "HELL");
  accept(loop, {4,0,0,0}, 1500); tick(loop, 1500);
  assert(!loop.hostActive() && shown == "PAKr");

  FrontPanel interval; accept(interval, message("HELLO", 2), 1000);
  for (uint32_t now = 1000; now <= 1480; now += 80) tick(interval, now);
  assert(interval.hostActive() && shown == "PAKr");
  tick(interval, 2460); assert(shown == "PAKr");
  tick(interval, 2480); assert(shown == "HELL");

  FrontPanel shortText; accept(shortText, message("HI"), 1000);
  tick(shortText, 1000); assert(shown == "HI");
  tick(shortText, 1160); assert(!shortText.hostActive());
  accept(shortText, message("HI", 0, 0), 2000);
  tick(shortText, 9000); assert(shortText.hostActive() && shown == "HI");
  accept(shortText, message("HI", 1), 10000);
  tick(shortText, 20000); assert(shortText.hostActive() && shown == "HI");

  // Numeric presentation replaces rather than hides behind a previous loop.
  FrontPanel number; accept(number, message("HELLO", 1), 1000);
  accept(number, {42,0}, 1100); tick(number, 1100);
  assert(!number.hostActive() && shown == "42");
  tick(number, 4100); assert(shown == "PAKr");

  FrontPanel rawPanel; accept(rawPanel, {5,80,0,4,0x20,160,0,0,1,2,4,8}, 1000);
  tick(rawPanel, 1000); assert(isRaw && (raw == std::array<uint8_t,4>{1,2,4,8}));
  // Raw forced scroll keeps masks raw and pads with unlit cells, not ASCII spaces.
  accept(rawPanel, {5,80,0,4,0xa0,160,0,0,1,2,4,8}, 2000);
  tick(rawPanel, 2080); assert(isRaw && (raw == std::array<uint8_t,4>{2,4,8,0}));
  tick(rawPanel, 2100, true); assert(shown == "Prog");

  // Signed deadline arithmetic continues to work across millis() rollover.
  FrontPanel rollover; accept(rollover, message("HI"), 0xfffffff0U);
  tick(rollover, 0xfffffff0U); assert(rollover.hostActive());
  tick(rollover, 0x90U); assert(!rollover.hostActive());
  auto invalid = message("HI", 3);
  assert(!rollover.acceptDisplayPayload(invalid.data(), invalid.size(), 0));
  invalid = message("HI", 2, 160, 0);
  assert(!rollover.acceptDisplayPayload(invalid.data(), invalid.size(), 0));
  invalid = {5,80,0,40,0,0,0,0};
  assert(!rollover.acceptDisplayPayload(invalid.data(), invalid.size(), 0));
  std::cout << "Front-panel one-shot, loop, interval, release, number override, raw cells, priority, bounds and rollover passed.\n";
}
