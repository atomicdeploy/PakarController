#pragma once

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include "../../esp32s3/src/BoardPins.h"
#endif
#include <FastLED.h>
#if defined(ARDUINO_ARCH_ESP32)
#include "../../esp32s3/src/LedOutput.h"
#endif

#include <Protocol.h>
#include <CellBatch.h>
#include <TableCellMapping.h>
#include "UpgradeLifecycle.h"

#ifndef PAKARCONTROLLER_SECTORS_PER_CELL
#define PAKARCONTROLLER_SECTORS_PER_CELL 6
#endif
#ifndef PAKAR_LED_START_ENABLED
#define PAKAR_LED_START_ENABLED 1
#endif
#ifndef PAKAR_LED_START_CELL_CHASE
#define PAKAR_LED_START_CELL_CHASE 0
#endif
static_assert(PAKAR_LED_START_CELL_CHASE==0 || PAKAR_LED_START_CELL_CHASE==1,"Cell chase startup must be boolean");
static_assert(PAKAR_LED_START_ENABLED==0 || PAKAR_LED_START_ENABLED==1,"LED startup policy must be boolean");

class LedGrid {
 public:
  enum Pattern : uint8_t {
    kPatternNone,
    kPatternRainbow,
    kPatternFire,
    kPatternCellChase,
  };

  static constexpr uint8_t kCells = 80;
  static constexpr uint8_t kSectorsPerCell = PAKARCONTROLLER_SECTORS_PER_CELL;
  static constexpr uint16_t kPixels = kCells * kSectorsPerCell;
#if defined(ARDUINO_ARCH_ESP32)
  static_assert(kPixels==PakarS3::LedOutput::pixels,"S3 output profile must match the full pixel buffer");
#endif

  void begin(bool publish = true);
  bool readyToShow(uint32_t now) const;
  bool needsRender(uint32_t now) const;
  void service(uint32_t now);
  bool setCell(uint8_t cell, BoardCore::Protocol::CellState state,
               BoardCore::Protocol::Effect effect);
  bool setCells(const BoardCore::CellBatch &selection, BoardCore::Protocol::CellState state,
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
  bool stagePixels(uint16_t offset, const uint8_t *rgb, uint8_t count,uint64_t owner=0);
  void discardStaged(uint64_t owner){if(staging_&&stageOwner_==owner){staging_=false;dirty_=true;}}
  bool canStage(uint64_t owner=0);
  bool commitStaged(uint64_t owner=0);
  // One loop-owned diagnostic transaction, bypassing logical cell mapping.
  bool diagnostic(uint16_t index, uint8_t count, uint32_t rgb, bool off);
  using PanelObserver=void (*)(uint8_t kind,uint16_t value,uint8_t detail);
  void setPanelObserver(PanelObserver observer){panelObserver_=observer;}
  bool diagnosticActive()const{return diagnosticActive_;}
  uint16_t diagnosticPixel()const{return diagnosticPixel_;}
  void setOutputEnabled(bool enabled);
  bool outputEnabled()const{return outputEnabled_;}
  void setStaticMode(bool enabled);
  bool staticMode()const{return staticMode_;}
  String outputJson(bool compact=false)const;
  String outputCapabilitiesJson()const;
  bool requestSignalCapture(){
#if defined(ARDUINO_ARCH_ESP32)
    return outputEnabled_ && output_.requestSignalCapture();
#else
    return false;
#endif
  }
  // A loop-thread snapshot of the last committed command RGB buffer, before
  // FastLED brightness/correction. Never expose an uncommitted streamed chunk.
  String pixelFrameJson() const;
  void presentMaintenance(const PakarUpdate::Lifecycle &lifecycle,uint32_t now);
  bool maintenancePresented()const;

 private:
  void showStaged(); // Loop renderer only; external commits must validate ownership.
  static CRGB colorFor(BoardCore::Protocol::CellState state);
  void renderCell(uint8_t index);
  void renderPattern(uint32_t now);
  void stopPattern(const char *reason = "stop-pattern");
  void changePattern(Pattern pattern, const char *reason);
  void renderChaseStep(uint8_t physical);

  CRGB pixels_[kPixels];
  CRGB staged_[kPixels];
  // Last loop-published command snapshot, independent of mutable render/stage
  // buffers. Driver transmission/physical output remain separately observed.
  CRGB published_[kPixels];
  uint8_t publishedPhysical_[kCells] = {};
  uint8_t publishedBrightness_ = 255;
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::LedOutput output_;
#endif
  CRGB current_[kCells];
  CRGB target_[kCells];
  CRGB transitionFrom_[kCells];
  uint32_t transitionAt_[kCells] = {};
  bool transitioning_[kCells] = {};
  uint8_t heat_[kPixels] = {};
  uint8_t states_[kCells] = {};
  uint8_t physicalCells_[kCells] = {};
  uint8_t brightness_ = 255;
  uint32_t lastFrameAt_ = 0;
  uint8_t hue_ = 0;
  uint8_t rainbowBaseHue_ = 0;
  uint32_t rainbowStartedAt_ = 0;
  Pattern pattern_ = kPatternNone;
  uint8_t chaseStep_ = 0, chaseCell_ = 0;
  bool chaseFirstFrame_ = false;
  uint32_t chaseAt_ = 0;
  static constexpr uint32_t kChaseIntervalMs = 750;
  // Loop-owned diagnostic only: constant reason pointers, no allocation,
  // persistence or logging in the renderer. Revision is the last published
  // frame when the logical effect changed, not a claim of wire completion.
  struct EffectChange {
    const char *reason = nullptr;
    Pattern previous = kPatternNone, next = kPatternNone;
    uint32_t at = 0, revision = 0;
  } lastEffectChange_;
  bool dirty_ = true;
  PanelObserver panelObserver_=nullptr;
  bool diagnosticActive_=false;
  uint16_t diagnosticPixel_=0;
  uint8_t diagnosticCount_=1;
  uint32_t diagnosticRgb_=0xffffff,diagnosticRevision_=0;
  bool diagnosticOff_=false;
  void clearDiagnostic(){if(diagnosticActive_){diagnosticActive_=false;++diagnosticRevision_;}}
  bool staticMode_ = true;
  bool staging_ = false;
  bool outputEnabled_ = PAKAR_LED_START_ENABLED != 0;
  uint32_t stagingAt_ = 0;
  uint64_t stageOwner_ = 0;
  uint32_t frameRevision_ = 0;
  bool maintenanceActive_=false,maintenanceBlack_=false;
  PakarUpdate::Visual maintenanceVisual_=PakarUpdate::Visual::None;
  CRGB maintenanceFrom_[kPixels];
#ifndef PAKAR_LED_FRAME_INTERVAL_MS
#define PAKAR_LED_FRAME_INTERVAL_MS 0
#endif
  // Zero means there is no application-imposed frame-rate ceiling.  The LED
  // transport itself determines the maximum rate (about 69 Hz for 480 RGB
  // pixels at 800 kbit/s).  Slower diagnostic builds may override this macro.
  static constexpr uint8_t kFrameIntervalMs = PAKAR_LED_FRAME_INTERVAL_MS;
};
