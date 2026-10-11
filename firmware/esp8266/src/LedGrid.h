#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include <Protocol.h>

#ifndef PAKARCONTROLLER_SECTORS_PER_CELL
#define PAKARCONTROLLER_SECTORS_PER_CELL 6
#endif

class LedGrid {
 public:
  enum Pattern : uint8_t {
    kPatternNone,
    kPatternRainbow,
    kPatternFire,
  };

  static constexpr uint8_t kCells = 80;
  static constexpr uint8_t kSectorsPerCell = PAKARCONTROLLER_SECTORS_PER_CELL;
  static constexpr uint16_t kPixels = kCells * kSectorsPerCell;

  void begin();
  bool readyToShow(uint32_t now) const;
  bool needsRender(uint32_t now) const;
  void service(uint32_t now);
  bool setCell(uint8_t cell, BoardCore::Protocol::CellState state,
               BoardCore::Protocol::Effect effect);
  void fill(BoardCore::Protocol::CellState state);
  void resetWhite();
  void showWinner(uint8_t winner);
  void setPattern(Pattern pattern);
  Pattern pattern() const { return pattern_; }
  const char *patternName() const;
  void setBrightness(uint8_t value);
  uint8_t brightness() const { return brightness_; }
  BoardCore::Protocol::CellState state(uint8_t cell) const;
  void setPhysicalMapping(const uint8_t *physicalCells, uint8_t count);
  uint8_t physicalCell(uint8_t logicalCell) const;
  bool stagePixels(uint16_t offset, const uint8_t *rgb, uint8_t count);
  void showStaged();
  // A loop-thread snapshot of the last committed command RGB buffer, before
  // FastLED brightness/correction. Never expose an uncommitted streamed chunk.
  String pixelFrameJson() const;

 private:
  static CRGB colorFor(BoardCore::Protocol::CellState state);
  void renderCell(uint8_t index);
  void renderPattern();
  void stopPattern();

  CRGB pixels_[kPixels];
  CRGB current_[kCells];
  CRGB target_[kCells];
  uint8_t heat_[kPixels] = {};
  uint8_t states_[kCells] = {};
  uint8_t physicalCells_[kCells] = {};
  uint8_t brightness_ = 255;
  uint32_t lastFrameAt_ = 0;
  uint8_t hue_ = 0;
  Pattern pattern_ = kPatternNone;
  bool dirty_ = true;
  bool staging_ = false;
  uint32_t frameRevision_ = 0;
#ifndef PAKAR_LED_FRAME_INTERVAL_MS
#define PAKAR_LED_FRAME_INTERVAL_MS 0
#endif
  // Zero means there is no application-imposed frame-rate ceiling.  The LED
  // transport itself determines the maximum rate (about 69 Hz for 480 RGB
  // pixels at 800 kbit/s).  Slower diagnostic builds may override this macro.
  static constexpr uint8_t kFrameIntervalMs = PAKAR_LED_FRAME_INTERVAL_MS;
};
