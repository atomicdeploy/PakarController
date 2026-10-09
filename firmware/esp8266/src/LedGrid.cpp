#include "LedGrid.h"

namespace {
constexpr uint8_t kLedDataPin = D2;

uint8_t approach(uint8_t value, uint8_t target) {
  if (value == target) return value;
  const uint8_t step = 8;
  if (value < target) return static_cast<uint8_t>(min<int>(target, value + step));
  return static_cast<uint8_t>(max<int>(target, value - step));
}
}

void LedGrid::begin() {
  FastLED.addLeds<WS2811, kLedDataPin, RGB>(pixels_, kPixels)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness_);
  resetWhite();
  for (uint8_t i = 0; i < kCells; ++i) current_[i] = target_[i];
  showStaged();
}

CRGB LedGrid::colorFor(BoardCore::Protocol::CellState state) {
  switch (state) {
    case BoardCore::Protocol::kCellRed: return CRGB::Red;
    case BoardCore::Protocol::kCellOff: return CRGB::Black;
    default: return CRGB::White;
  }
}

bool LedGrid::setCell(uint8_t cell, BoardCore::Protocol::CellState state,
                      BoardCore::Protocol::Effect effect) {
  if (cell < 1 || cell > kCells || state > BoardCore::Protocol::kCellOff) return false;
  stopPattern();
  const uint8_t index = cell - 1;
  states_[index] = state;
  target_[index] = colorFor(state);
  if (effect == BoardCore::Protocol::kEffectImmediate) current_[index] = target_[index];
  else if (effect == BoardCore::Protocol::kEffectPulse) current_[index] = CRGB::Black;
  dirty_ = true;
  return true;
}

void LedGrid::resetWhite() {
  pattern_ = kPatternNone;
  for (uint8_t i = 0; i < kCells; ++i) {
    states_[i] = BoardCore::Protocol::kCellWhite;
    target_[i] = CRGB::White;
    current_[i] = CRGB::White;
    renderCell(i);
  }
  dirty_ = true;
}

void LedGrid::showWinner(uint8_t winner) {
  if (winner < 1 || winner > 9) return;
  pattern_ = kPatternNone;
  for (uint8_t i = 0; i < kCells; ++i) {
    const uint8_t group = static_cast<uint8_t>((static_cast<uint16_t>(i) * 9U) /
                                               kCells) + 1U;
    states_[i] = group == winner ? BoardCore::Protocol::kCellRed
                                 : BoardCore::Protocol::kCellOff;
    target_[i] = colorFor(static_cast<BoardCore::Protocol::CellState>(states_[i]));
    current_[i] = target_[i];
    renderCell(i);
  }
  dirty_ = true;
}

void LedGrid::setPattern(Pattern pattern) {
  if (pattern == kPatternNone) {
    stopPattern();
    dirty_ = true;
    return;
  }
  pattern_ = pattern;
  dirty_ = true;
}

const char *LedGrid::patternName() const {
  switch (pattern_) {
    case kPatternRainbow: return "rainbow";
    case kPatternFire: return "fire";
    default: return "none";
  }
}

void LedGrid::stopPattern() {
  if (pattern_ == kPatternNone) return;
  pattern_ = kPatternNone;
  for (uint8_t i = 0; i < kCells; ++i) {
    current_[i] = target_[i];
    renderCell(i);
  }
}

void LedGrid::renderCell(uint8_t index) {
  const uint16_t first = static_cast<uint16_t>(index) * kSectorsPerCell;
  for (uint8_t sector = 0; sector < kSectorsPerCell; ++sector) {
    pixels_[first + sector] = current_[index];
  }
}

void LedGrid::service(uint32_t now) {
  if (static_cast<uint32_t>(now - lastFrameAt_) < 20) return;
  lastFrameAt_ = now;
  if (pattern_ != kPatternNone) {
    renderPattern();
    showStaged();
    return;
  }
  for (uint8_t i = 0; i < kCells; ++i) {
    const CRGB before = current_[i];
    current_[i].r = approach(current_[i].r, target_[i].r);
    current_[i].g = approach(current_[i].g, target_[i].g);
    current_[i].b = approach(current_[i].b, target_[i].b);
    if (current_[i] != before) {
      renderCell(i);
      dirty_ = true;
    }
  }
  if (dirty_) showStaged();
}

void LedGrid::renderPattern() {
  ++hue_;
  switch (pattern_) {
    case kPatternRainbow:
      fill_rainbow(pixels_, kPixels, hue_, 7);
      break;
    case kPatternFire:
      for (uint16_t i = 0; i < kPixels; ++i) {
        heat_[i] = qsub8(heat_[i], random8(0, 3));
      }
      for (uint16_t i = kPixels - 1; i >= 2; --i) {
        heat_[i] = static_cast<uint8_t>((heat_[i - 1] + heat_[i - 2] +
                                         heat_[i - 2]) / 3);
      }
      if (random8() < 120) {
        const uint8_t spark = random8(min<uint16_t>(7, kPixels));
        heat_[spark] = qadd8(heat_[spark], random8(160, 255));
      }
      for (uint16_t i = 0; i < kPixels; ++i) pixels_[i] = HeatColor(heat_[i]);
      break;
    default:
      break;
  }
}

void LedGrid::setBrightness(uint8_t value) {
  brightness_ = value;
  FastLED.setBrightness(value);
  dirty_ = true;
}

BoardCore::Protocol::CellState LedGrid::state(uint8_t cell) const {
  if (cell < 1 || cell > kCells) return BoardCore::Protocol::kCellOff;
  return static_cast<BoardCore::Protocol::CellState>(states_[cell - 1]);
}

bool LedGrid::stagePixels(uint16_t offset, const uint8_t *rgb, uint8_t count) {
  if (offset + count > kPixels || rgb == nullptr) return false;
  pattern_ = kPatternNone;
  for (uint8_t i = 0; i < count; ++i) {
    pixels_[offset + i] = CRGB(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
  }
  dirty_ = true;
  return true;
}

void LedGrid::showStaged() {
  FastLED.show();
  dirty_ = false;
}
