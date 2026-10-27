// Executes production FrontPanel.cpp against a display transport double.
#include "FrontPanel.h"
#include "SevenSegments.h"
#include "../firmware/esp/src/UpdateProgress.h"
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
  // The ESP's actual progress encoder is accepted by the unchanged AVR owner.
  PakarUpdate::Panel updateEncoder; uint8_t updatePayload[12];
  assert(updateEncoder.next(1000,true,true,"P042",updatePayload));
  FrontPanel update;
  assert(update.acceptDisplayPayload(updatePayload,sizeof(updatePayload),1000));
  tick(update,1000); assert(shown=="P042" && update.hostActive());
  tick(update,1100,true); assert(shown=="Prog"); // ISP safety keeps priority.
  tick(update,2200); assert(shown=="C001" && !update.hostActive());
  // Long text scrolls once, including one blank frame, then releases locally.
  FrontPanel once; accept(once, message("HELLO"), 1000);
  tick(once, 1000); assert(shown == "HELL");
  tick(once, 1400); // missed ticks advance one frame, never skip the terminal blank
  for (uint32_t now = 1480; now <= 1720; now += 80) tick(once, now);
  assert(shown == "    " && once.hostActive());
  tick(once, 1800); assert(!once.hostActive() && shown == "C001");
  tick(once, 9000); assert(!once.hostActive());

  FrontPanel legacy; accept(legacy, {0,80,0,5,'H','E','L','L','O'}, 1000);
  for (uint32_t now = 1000; now <= 1480; now += 80) tick(legacy, now);
  assert(!legacy.hostActive());

  FrontPanel loop; accept(loop, message("HELLO", 1), 1000);
  for (uint32_t now = 1000; now <= 1480; now += 80) tick(loop, now);
  assert(loop.hostActive() && shown == "HELL");
  accept(loop, {4,0,0,0}, 1500); tick(loop, 1500);
  assert(!loop.hostActive() && shown == "C001");

  FrontPanel interval; accept(interval, message("HELLO", 2), 1000);
  for (uint32_t now = 1000; now <= 1480; now += 80) tick(interval, now);
  assert(interval.hostActive() && shown == "C001");
  tick(interval, 2460); assert(shown == "C001");
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
  tick(number, 4100); assert(shown == "C001");

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
  FrontPanel captured;
  accept(captured,{3,0x34,0x12,4,'H','O','S','T'},3000); tick(captured,9000);
  assert(captured.captured() && captured.capturedMeta()==0x1234 && shown=="HOST");
  accept(captured,{4,0,0,0},9100); assert(!captured.captured());
  captured.setMenu(2,true,257); tick(captured,9200); assert(shown=="KEYS");
  tick(captured,9900); assert(shown=="257");
  captured.setMenu(2,true,258); // Value refresh must not restart the label clock.
  tick(captured,10000); assert(shown=="258");
  tick(captured,11000); assert(shown=="KEYS");
  tick(captured,11100,true); assert(shown=="Prog");
  FrontPanel full; accept(full,message("0123456789012345678901234567890123456789"),1000);
  tick(full,1000); assert(shown=="0123");
  assert(!rollover.acceptDisplayPayload(invalid.data(), invalid.size(), 0));
  invalid = message("HI", 2, 160, 0);
  assert(!rollover.acceptDisplayPayload(invalid.data(), invalid.size(), 0));
  invalid = {5,80,0,40,0,0,0,0};
  assert(!rollover.acceptDisplayPayload(invalid.data(), invalid.size(), 0));
  FrontPanel esp;
  esp.setEspMenu(true,0,BoardCore::Protocol::EspPanel::Current,3,"ledboard",1000);
  tick(esp,1000); assert(shown=="nEt ");
  tick(esp,1700); assert(shown=="APCL");
  esp.setEspMenu(true,0,BoardCore::Protocol::EspPanel::Current,3,"ledboard",1750);
  tick(esp,1750); assert(shown=="APCL"); // Offer refresh never restarts label.
  esp.setEspMenu(true,0,BoardCore::Protocol::EspPanel::Pending,3,"ledboard",1800);
  tick(esp,1800); assert(shown=="buSY");
  esp.setEspMenu(true,0,BoardCore::Protocol::EspPanel::Failed,3,"ledboard",1900);
  tick(esp,1900); assert(shown=="Err ");
  tick(esp,2000,true); assert(shown=="Prog");
  esp.setEspMenu(true,2,BoardCore::Protocol::EspPanel::Saved,0,"ledboard",2100);
  tick(esp,2100); assert(shown=="SEnt"); // Queue acceptance isn't completed scan.
  esp.setEspMenu(true,4,BoardCore::Protocol::EspPanel::Current,0,"ledboard",3000);
  tick(esp,3700); assert(shown=="ledb");
  tick(esp,9000); assert(shown=="nAME");
  tick(esp,9700); assert(shown=="oard");
  tick(esp,12000); assert(shown=="nAME");
  tick(esp,12700); assert(shown=="oard"); // One pass, then alternate context/last window.
  esp.setEspMenu(false,4,0,0,nullptr,12800);
  tick(esp,12800); assert(shown=="C001");

  FrontPanel resting;
  resting.service(1000,BoardCore::Protocol::kModeCellSelect,80,0,false);
  assert(shown=="C080");
  resting.service(1020,BoardCore::Protocol::kModeFirstWins,80,0,false);
  assert(shown=="rEAd");
  resting.service(1040,BoardCore::Protocol::kModeFirstWins,80,9,false);
  assert(shown=="W009"); // The transport glyph uses U-shaped approximation of W.
  resting.service(1060,BoardCore::Protocol::kModeSnake,80,0,false);
  assert(shown=="SnAk");
  resting.service(1080,static_cast<BoardCore::Protocol::GameMode>(255),80,0,false);
  assert(shown=="    "); // Unknown/disabled mode must not invent an idle state.

  FrontPanel manual;
  accept(manual,message("nEt ",0,1200),1000);
  manual.setMenu(0,true,80); tick(manual,1000); assert(shown=="CELL");
  tick(manual,1700); assert(shown=="80");
  accept(manual,message("nEt ",0,1200),1800);
  tick(manual,1800); assert(shown=="80"); // Automatic renewal cannot hide menu.
  tick(manual,2800); assert(shown=="CELL");
  accept(manual,{3,0,0,4,'H','O','S','T'},3000);
  tick(manual,3000); assert(shown=="HOST" && manual.captured());
  tick(manual,3100,true); assert(shown=="Prog");
  accept(manual,{4,0,0,0},3200); tick(manual,3200); assert(shown=="CELL");

  // Existing raw scheduled payloads are sufficient for a 2-second notice or
  // ESP-owned blink: all-zero bytes intentionally blank, then expire locally.
  FrontPanel blank;
  accept(blank,{5,80,0,4,0x20,0xd0,0x07,0,0,0,0,0},1000);
  tick(blank,1000); assert(isRaw && (raw==std::array<uint8_t,4>{0,0,0,0}));
  tick(blank,2980); assert(blank.hostActive() && isRaw);
  tick(blank,3000); assert(!blank.hostActive() && shown=="C001");
  // Append-only maintenance option: finite measured progress may cover a
  // settings menu; expiry reveals the menu, and Prog still wins immediately.
  FrontPanel maintenance;
  maintenance.setEspMenu(true,0,BoardCore::Protocol::EspPanel::Current,3,"ledboard",1000);
  accept(maintenance,{5,80,0,4,4,0xb0,4,0,'P','0','4','2'},1100);
  tick(maintenance,1100); assert(shown=="P042");
  tick(maintenance,1200,true); assert(shown=="Prog");
  tick(maintenance,2300); assert(shown!="P042");
  std::cout << "Front-panel one-shot, loop, interval, release, semantic resting state, menu label/value, raw notices, priority, bounds and rollover passed.\n";
}
