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
#include <GameRole.h>
#include <KeyPresentation.h>
#include <StateAppearance.h>
#include <MatrixFire.h>
#include <CellBatch.h>
#include <TableCellMapping.h>
#include <PixelDiagnostic.h>
#include <PixelGroupLayout.h>
#include <HostPresentation.h>
#include "UpgradeLifecycle.h"
#ifndef PAKAR_PIXEL_CORRECTIONS
#define PAKAR_PIXEL_CORRECTIONS 0
#endif
#if PAKAR_PIXEL_CORRECTIONS
#include <PixelCorrections.h>
#endif

#ifndef PAKARCONTROLLER_SECTORS_PER_CELL
#define PAKARCONTROLLER_SECTORS_PER_CELL BoardCore::GameRole::pixelsPerGroup
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

  static constexpr uint8_t kCells = BoardCore::GameRole::ledGroups;
  static constexpr uint8_t kSectorsPerCell = PAKARCONTROLLER_SECTORS_PER_CELL;
  static constexpr uint16_t kPixels = kCells * kSectorsPerCell;
  using GroupLayout=BoardCore::PixelGroupLayout<kCells,kSectorsPerCell,kPixels>;
  using Presentation=BoardCore::HostPresentation<kCells,kPixels>;
  void setPresentation(Presentation *value){presentation_=value;dirty_=true;}
  void setGamePresentation(Presentation::Layer *value){gameLayer_=value;dirty_=true;}
  void setGamePresentationEpoch(uint32_t value){gamePresentationAt_=value;}
  void presentationChanged(){dirty_=true;}
  bool presentationReady()const{return !staging_&&!maintenanceActive_&&!diagnosticActive_&&!previewActive(millis());}
  const GroupLayout &groupLayout()const{return groupLayout_;}
  uint32_t publishedGroupRevision()const{return publishedGroups_.revision();}
  bool canConfigureGroups()const{return !staging_&&!diagnosticActive_&&!previewActive(millis())&&!maintenanceActive_&&!(presentation_&&presentation_->active());}
  bool configureGroups(const GroupLayout &layout);
#if PAKAR_PIXEL_CORRECTIONS
  void setCorrections(const BoardCore::PixelCorrections<kPixels> *mapping){corrections_=mapping;dirty_=true;}
#endif
#if defined(ARDUINO_ARCH_ESP32)
  static_assert(kPixels==PakarS3::LedOutput::pixels,"S3 output profile must match the full pixel buffer");
#endif

  void begin(bool publish = true);
#if defined(ARDUINO_ARCH_ESP32)
  void setInitialBackend(uint8_t value){initialBackend_=static_cast<PakarS3::LedOutput::Backend>(value);}
  bool requestBackend(uint8_t value){const bool accepted=output_.requestBackend(static_cast<PakarS3::LedOutput::Backend>(value));if(accepted)dirty_=true;return accepted;}
  uint32_t backendRevision()const{return output_.backendRevision();}
  bool backendPending()const{return output_.switchPending();}
  int backendSwitchError()const{return output_.switchError();}
#endif
  void setBackendPersistence(bool saved,const char* error){backendSaved_=saved;backendSettingsError_=error;}
  bool readyToShow(uint32_t now) const;
  bool needsRender(uint32_t now) const;
  void service(uint32_t now);
  bool setCell(uint8_t cell, BoardCore::Protocol::CellState state,
               BoardCore::Protocol::Effect effect);
  bool setCells(const BoardCore::CellBatch &selection, BoardCore::Protocol::CellState state,
                BoardCore::Protocol::Effect effect);
  bool setAssignments(const uint8_t *cells, const uint8_t *states, uint8_t count);
  void setRedLock(bool enabled);
  bool redLock() const { return redLock_; }
  bool permits(uint8_t cell, BoardCore::Protocol::CellState state) const {
    return cell >= 1 && cell <= kCells && state <= BoardCore::Protocol::kCellWon &&
      !(redLock_ && states_[cell-1] == BoardCore::Protocol::kCellRed && state == BoardCore::Protocol::kCellWhite);
  }
  bool fill(BoardCore::Protocol::CellState state);
  bool configureAppearance(const BoardCore::StateAppearance &value);
  const BoardCore::StateAppearance &appearance()const{return appearance_;}
  bool preview(const BoardCore::CellBatch &selection,uint32_t rgb,uint16_t durationMs);
  void cancelPreview(){if(previewDuration_){previewDuration_=0;dirty_=true;}}
  bool previewActive(uint32_t now)const{return previewDuration_ && uint32_t(now-previewAt_)<previewDuration_;}
  uint32_t previewRgb()const{return previewRgb_;}
  bool resetWhite();
  void showWinner(uint8_t winner,bool restartPresentation=true);
  void configureKeyPresentation(const BoardCore::KeyPresentation &value){keyPresentation_=value;dirty_=true;}
  void setKeyEffects(bool enabled){keyEffects_=enabled;if(!enabled&&keyWinner_)for(uint8_t i=0;i<kCells;++i)renderCell(i);dirty_=true;}
  void configureKeyMotion(uint16_t periodMs,bool neighbors,bool invert){
    keyChasePeriodMs_=periodMs;keyChaseNeighbors_=neighbors;keyChaseInvert_=invert;dirty_=true;
  }
  void configureKeys(uint16_t mask,uint32_t idle,uint32_t win,uint32_t lose){
    keyMask_=mask;keyIdle_=idle;keyWin_=win;keyLose_=lose;resetWhite();
  }
  bool setPattern(Pattern pattern);
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
  bool diagnostic(uint16_t index, uint8_t count, uint32_t rgb, bool off,
      const BoardCore::PixelDiagnostic &options=BoardCore::PixelDiagnostic{});
  using PanelObserver=void (*)(uint8_t kind,uint16_t value,uint8_t detail);
  void setPanelObserver(PanelObserver observer){panelObserver_=observer;}
  bool diagnosticActive()const{return diagnosticActive_;}
  uint16_t diagnosticPixel()const{return diagnosticPixel_;}
  void setOutputEnabled(bool enabled);
  bool outputEnabled()const{return outputEnabled_;}
  bool sceneOutputEnabled()const{return diagnosticActive_?diagnosticOutputBefore_:outputEnabled_;}
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
  String pixelFrameJson(bool compact = false) const;
  uint32_t frameRevision() const { return frameRevision_; }
  void presentMaintenance(const PakarUpdate::Lifecycle &lifecycle,uint32_t now);
  bool maintenancePresented()const;

 private:
  void retirePresentation(){if(presentation_&&presentation_->retire())dirty_=true;}
  bool presentationAnimated()const{return presentation_&&presentation_->needsFrame(millis(),lastFrameAt_);}
  Presentation *presentation_=nullptr;
  Presentation::Layer *gameLayer_=nullptr;
  uint32_t gamePresentationAt_=0;
  uint32_t publishedPresentationRevision_=0;
  const CRGB *corrected(const CRGB *raw) {
#if PAKAR_PIXEL_CORRECTIONS
    if(corrections_){
      corrections_->render(raw,corrected_,CRGB(CRGB::Black));
      for(uint16_t i=groupLayout_.used();i<kPixels;++i)corrected_[i]=CRGB::Black;
      return corrected_;
    }
#endif
    return raw;
  }
#if PAKAR_PIXEL_CORRECTIONS
  const BoardCore::PixelCorrections<kPixels> *corrections_=nullptr;
  CRGB corrected_[kPixels];
#endif
  void showStaged(); // Loop renderer only; external commits must validate ownership.
  CRGB colorFor(BoardCore::Protocol::CellState state) const;
  void renderStateEffects(uint32_t now);
  void renderCell(uint8_t index);
  void renderPattern(uint32_t now);
  void renderKeyEffects(uint32_t now);
  void stopPattern(const char *reason = "stop-pattern");
  void changePattern(Pattern pattern, const char *reason);
  void renderChaseStep(uint8_t physical);
  uint8_t chaseGroups()const{
#if PAKAR_FIRST_WINS_ENABLED
    uint8_t n=0;for(uint8_t k=0;k<9;++k)if(keyMask_&(1U<<k))n+=2;return n;
#else
    return kCells;
#endif
  }

  CRGB pixels_[kPixels];
  CRGB staged_[kPixels];
  // Last loop-published command snapshot, independent of mutable render/stage
  // buffers. Driver transmission/physical output remain separately observed.
  CRGB published_[kPixels];
  GroupLayout groupLayout_,publishedGroups_;
  uint8_t publishedPhysical_[kCells] = {};
  uint8_t publishedBrightness_ = 255;
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::LedOutput output_;
  PakarS3::LedOutput::Backend initialBackend_=PakarS3::LedOutput::defaultBackend();
#endif
  bool backendSaved_=false;
  const char* backendSettingsError_="";
  CRGB current_[kCells];
  CRGB target_[kCells];
  CRGB transitionFrom_[kCells];
  uint32_t transitionAt_[kCells] = {};
  bool transitioning_[kCells] = {};
  uint8_t transitionEffects_[kCells] = {};
  BoardCore::MatrixFire<PAKAR_TABLE_SELECTION?20:1,PAKAR_TABLE_SELECTION?8:kPixels> fire_;
  uint8_t states_[kCells] = {};
  BoardCore::StateAppearance appearance_;
  BoardCore::CellBatch previewSelection_;
  uint32_t previewRgb_=0,previewAt_=0;
  uint16_t previewDuration_=0;
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
  bool redLock_ = false;
  uint16_t keyMask_=511;
  bool keyEffects_=false;
  uint16_t keyChasePeriodMs_=900;
  bool keyChaseNeighbors_=true,keyChaseInvert_=false;
  uint8_t keyWinner_=0;
  BoardCore::KeyPresentation keyPresentation_;
  uint32_t keyWinnerAt_=0;
  uint32_t keyIdle_=0xffffff,keyWin_=0x00ff00,keyLose_=0xff0000;
  PanelObserver panelObserver_=nullptr;
  bool diagnosticActive_=false;
  uint16_t diagnosticPixel_=0;
  uint8_t diagnosticCount_=1;
  uint32_t diagnosticRgb_=0xffffff,diagnosticRevision_=0;
  bool diagnosticOff_=false;
  BoardCore::PixelDiagnostic diagnosticOptions_;
  uint32_t diagnosticAt_=0;
  bool diagnosticOutputBefore_=true;
  void clearDiagnostic(){if(diagnosticActive_){diagnosticActive_=false;outputEnabled_=diagnosticOutputBefore_;dirty_=true;++diagnosticRevision_;}}
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
