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
  void service(uint32_t now);
  bool setCell(uint8_t cell, BoardCore::Protocol::CellState state,
               BoardCore::Protocol::Effect effect);
  void resetWhite();
  void showWinner(uint8_t winner);
  void setPattern(Pattern pattern);
  Pattern pattern() const { return pattern_; }
  const char *patternName() const;
  void setBrightness(uint8_t value);
  uint8_t brightness() const { return brightness_; }
  BoardCore::Protocol::CellState state(uint8_t cell) const;
  bool stagePixels(uint16_t offset, const uint8_t *rgb, uint8_t count);
  void showStaged();

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
  uint8_t brightness_ = 48;
  uint32_t lastFrameAt_ = 0;
  uint8_t hue_ = 0;
  Pattern pattern_ = kPatternNone;
  bool dirty_ = true;
};
