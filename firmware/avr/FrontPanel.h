#pragma once

#include <Arduino.h>
#include <Protocol.h>

// TM1637 presentation ownership adapted from PCController's
// FrontPanelRuntime.inc.h. It keeps local game state truthful while allowing a
// bounded host lease for static text, marquee text, and raw segment frames.
class FrontPanel {
 public:
  void begin(uint32_t now);
  void service(uint32_t now, BoardCore::Protocol::GameMode mode,
               uint8_t selectedCell, uint8_t winner, bool programming);
  void showTransientNumber(int16_t value, uint32_t now,
                           uint16_t durationMs = 1800);
  bool acceptDisplayPayload(const uint8_t *payload, uint8_t length,
                            uint32_t now);
  void release();
  bool hostActive() const { return hostTextActive_; }
  bool captured() const { return captured_; }
  uint16_t capturedMeta() const { return capturedMeta_; }
  void setMenu(uint8_t page, bool active, int16_t value) {
    menuPage_ = page; menuActive_ = active; menuValue_ = value;
  }
  void setEspMenu(bool active, uint8_t page, uint8_t notice, uint8_t value,
                  const char *hostname, uint32_t now);

 private:
  void showLocal(BoardCore::Protocol::GameMode mode, uint8_t selectedCell,
                 uint8_t winner);
  bool showHostText(uint32_t now);
  void showEspMenu(uint32_t now);
  bool timeReached(uint32_t now, uint32_t deadline) const {
    return static_cast<int32_t>(now - deadline) >= 0;
  }

  char hostText_[41] = {};
  uint8_t hostTextLength_ = 0;
  uint8_t scrollIndex_ = 0;
  uint16_t stepMs_ = 260;
  uint16_t holdMs_ = 0;
  uint8_t options_ = 0;
  uint8_t intervalSeconds_ = 0;
  uint32_t nextAt_ = 0;
  uint32_t transientEndsAt_ = 0;
  int16_t transientNumber_ = 0;
  uint32_t lastRefreshAt_ = 0;
  uint32_t animationStartedAt_ = 0;
  bool hostTextActive_ = false;
  bool transientActive_ = false;
  bool animationActive_ = false;
  bool captured_ = false;
  uint16_t capturedMeta_ = 0;
  uint8_t menuPage_ = 0;
  bool menuActive_ = false;
  int16_t menuValue_ = 0;
  const char *espHostname_ = nullptr; // Runtime-owned stable storage, no heap/copy.
  uint32_t espPageAt_ = 0;
  uint8_t espPage_ = 0, espNotice_ = 0, espValue_ = 0;
  bool espActive_ = false;
};

extern FrontPanel frontPanel;
