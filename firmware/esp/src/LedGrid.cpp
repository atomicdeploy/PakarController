#include "LedGrid.h"
#include "WinnerPresentation.h"
#include <StateMotion.h>
#include <string.h>

namespace {
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t kLedDataPin = PAKAR_S3_LED_DATA;
#else
constexpr uint8_t kLedDataPin = D2;
#endif

const char *effectName(LedGrid::Pattern pattern) {
  switch (pattern) {
    case LedGrid::kPatternRainbow: return "rainbow";
    case LedGrid::kPatternFire: return "fire";
    case LedGrid::kPatternCellChase: return "cell-chase";
    default: return "none";
  }
}
}

void LedGrid::begin(bool publish) {
  for (uint8_t i = 0; i < kCells; ++i) physicalCells_[i] = i + 1;
#if defined(ARDUINO_ARCH_ESP32)
  output_.begin(initialBackend_);
#else
  FastLED.addLeds<WS2811, kLedDataPin, RGB>(pixels_, kPixels)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness_);
#endif
  resetWhite();
  for (uint8_t i = 0; i < kCells; ++i) current_[i] = target_[i];
  if (publish) showStaged();
#if defined(ARDUINO_ARCH_ESP32)
  // Quiet commissioning still sends one complete black frame to clear latched
  // pixels; no white/effect output is enabled until explicitly requested.
  if(publish && !outputEnabled_)output_.submit(pixels_,brightness_,false,frameRevision_);
#endif
}

CRGB LedGrid::colorFor(BoardCore::Protocol::CellState state) const {
  const uint32_t rgb=appearance_.states[state<=BoardCore::Protocol::kCellWon?state:0].rgb;
  return CRGB(rgb>>16,rgb>>8,rgb);
}

bool LedGrid::configureAppearance(const BoardCore::StateAppearance &value) {
  if(!value.valid())return false;
  appearance_=value;
  if(PAKAR_TABLE_SELECTION)for(uint8_t i=0;i<kCells;++i){
    current_[i]=target_[i]=colorFor(static_cast<BoardCore::Protocol::CellState>(states_[i]));
    transitioning_[i]=false;renderCell(i);
  }
  dirty_=true;return true;
}

bool LedGrid::preview(const BoardCore::CellBatch &selection,uint32_t rgb,uint16_t durationMs) {
  if(!selection.count()||rgb>0xffffff||!durationMs||durationMs>10000||staging_||!outputEnabled_)return false;
  for(uint8_t id=kCells+1;id<=80;++id)if(selection.contains(id))return false;
  retirePresentation();
  previewSelection_=selection;previewRgb_=rgb;previewAt_=millis();previewDuration_=durationMs;
  dirty_=true;return true;
}

bool LedGrid::setCell(uint8_t cell, BoardCore::Protocol::CellState state,
                      BoardCore::Protocol::Effect effect) {
  if(cell<1||cell>kCells)return false;
  BoardCore::CellBatch selection;selection.add(cell);return setCells(selection,state,effect);
}

bool LedGrid::setCells(const BoardCore::CellBatch &selection,
                      BoardCore::Protocol::CellState state,BoardCore::Protocol::Effect effect) {
  if(!selection.count()||state>BoardCore::Protocol::kCellWon||effect>BoardCore::Protocol::kEffectPulse)return false;
  for(uint8_t id=1;id<=kCells;++id)if(selection.contains(id)&&!permits(id,state))return false;
  retirePresentation();
  BoardCore::CellBatch continuing;
  // A repeated Fade to the same target must not restart its clock. Independent
  // clients may send the same intent with new request IDs; stretching a fade
  // on each accepted command made it slow and visibly uneven. Pulse and a new
  // target still deliberately start a new transition from the published RGB.
  const bool preserveFade=!staticMode_ && effect==BoardCore::Protocol::kEffectFade &&
      pattern_==kPatternNone && !diagnosticActive_ && !staging_ && !previewDuration_;
  // Read actual last-published logical RGB before stopping a global pattern.
  // No yields, persistence, UART fan-out or output submission inside this loop.
  for(uint8_t id=1;id<=kCells;++id)if(selection.contains(id)){
    const uint8_t index=id-1,physical=publishedPhysical_[index];
    if(preserveFade && transitioning_[index] && transitionEffects_[index]==BoardCore::Protocol::kEffectFade &&
       states_[index]==state && target_[index]==colorFor(state)){
      continuing.add(id);continue;
    }
    transitionFrom_[index]=frameRevision_&&physical>=1&&physical<=kCells?
      published_[publishedGroups_.start(physical)]:current_[index];
  }
  stopPattern("cell-batch");
  const uint32_t started=millis();
  for(uint8_t id=1;id<=kCells;++id)if(selection.contains(id)){
    if(continuing.contains(id))continue;
    const uint8_t index=id-1;states_[index]=state;target_[index]=colorFor(state);transitionAt_[index]=started;
    transitionEffects_[index]=effect;
    transitioning_[index]=!staticMode_&&effect!=BoardCore::Protocol::kEffectImmediate;
    if(transitioning_[index]){
      if(effect==BoardCore::Protocol::kEffectPulse)transitionFrom_[index]=CRGB::Black;
      current_[index]=transitionFrom_[index];transitionAt_[index]=started;
    }else current_[index]=target_[index];
    renderCell(index);
  }
  clearDiagnostic();
  if(panelObserver_) {
    if(selection.count()==kCells)panelObserver_(12,state,0); // ALL, not misleading C001.
    else panelObserver_(1,selection.first(),uint8_t(state)|(uint8_t(effect)<<2));
  }
  dirty_=true;return true;
}

bool LedGrid::setAssignments(const uint8_t *cells,const uint8_t *states,uint8_t count) {
  if(!cells||!states||!count||count>kCells)return false;
  BoardCore::CellBatch seen;
  for(uint8_t i=0;i<count;++i) {
    if(!permits(cells[i],static_cast<BoardCore::Protocol::CellState>(states[i]))||seen.contains(cells[i]))return false;
    seen.add(cells[i]);
  }
  // One transaction, not80 pattern resets/AVR notices or output publications.
  retirePresentation();
  staging_=false;stopPattern("cell-assignments");clearDiagnostic();
  for(uint8_t i=0;i<count;++i){
    const uint8_t index=cells[i]-1;states_[index]=states[i];
    target_[index]=colorFor(static_cast<BoardCore::Protocol::CellState>(states[i]));
    current_[index]=target_[index];transitioning_[index]=false;renderCell(index);
  }
  dirty_=true;
  if(panelObserver_)panelObserver_(1,cells[0],states[0]);
  return true;
}

bool LedGrid::fill(BoardCore::Protocol::CellState state) {
  for(uint8_t id=1;id<=kCells;++id)if(!permits(id,state))return false;
  retirePresentation();
  clearDiagnostic();
  staging_ = false;
  changePattern(kPatternNone,"fill");
  for (uint8_t i = 0; i < kCells; ++i) {
    states_[i] = state;
    target_[i] = colorFor(state);
    current_[i] = target_[i];
    transitioning_[i] = false;
    renderCell(i);
  }
  dirty_ = true;
  if(panelObserver_)panelObserver_(12,state,0);
  return true;
}

bool LedGrid::resetWhite() {
  keyWinner_=0;
#if PAKAR_FIRST_WINS_ENABLED
  fill(BoardCore::Protocol::kCellWhite);
  for(uint8_t i=0;i<kCells;++i){current_[i]=target_[i]=CRGB(keyIdle_>>16,keyIdle_>>8,keyIdle_);renderCell(i);}
  return true;
#else
  return fill(BoardCore::Protocol::kCellWhite);
#endif
}

void LedGrid::showWinner(uint8_t winner,bool restartPresentation) {
#if PAKAR_FIRST_WINS_ENABLED
  if (winner < 1 || winner > 9) return;
  retirePresentation();
  keyWinner_=winner;if(restartPresentation)keyWinnerAt_=millis();
  clearDiagnostic();
  staging_ = false;
  changePattern(kPatternNone,"winner");
  for (uint8_t i = 0; i < kCells; ++i) {
    const uint8_t group = i / BoardCore::GameRole::groupsPerPlayer + 1U;
    // The table's three-state storage is not the game's color authority.
    // Winner identity reconstructs the game; pixel frames carry actual RGB.
    states_[i] = group==winner?BoardCore::Protocol::kCellWon:BoardCore::Protocol::kCellLost;
    const auto &appearance=keyPresentation_.groups[i];
    const uint32_t color=group == winner ? (appearance.overrideColors?appearance.win:keyWin_) : (appearance.overrideColors?appearance.lose:keyLose_);
    target_[i] = CRGB(color>>16,color>>8,color);
    current_[i] = target_[i];
    transitioning_[i] = false;
    renderCell(i);
  }
  dirty_ = true;
#else
  (void)winner;
  (void)restartPresentation;
#endif
}

void LedGrid::setRedLock(bool enabled) {
  redLock_=PAKAR_TABLE_SELECTION&&enabled;
  if(redLock_&&pattern_==kPatternCellChase){stopPattern("red-cell-lock");dirty_=true;}
}

bool LedGrid::setPattern(Pattern pattern) {
  if(redLock_&&pattern==kPatternCellChase)return false;
  retirePresentation();
  keyWinner_=0;
  clearDiagnostic();
  if(panelObserver_)panelObserver_(3,pattern,0);
  if (pattern == kPatternNone) {
    stopPattern("explicit-none");
    dirty_ = true;
    return true;
  }
  staging_ = false;
  changePattern(pattern,"explicit-pattern");
  if(pattern==kPatternCellChase){
    chaseStep_=1;chaseFirstFrame_=true;chaseAt_=millis();renderChaseStep(chaseStep_);
  }
  for(uint8_t i=0;i<kCells;++i)transitioning_[i]=false;
  dirty_ = true;
  return true;
}

const char *LedGrid::patternName() const {
  return effectName(pattern_);
}

void LedGrid::changePattern(Pattern pattern,const char *reason) {
  if(pattern_==pattern)return;
  if(pattern==kPatternRainbow){
    rainbowBaseHue_=hue_;
    rainbowStartedAt_=millis();
  }
  lastEffectChange_.reason=reason;
  lastEffectChange_.previous=pattern_;
  lastEffectChange_.next=pattern;
  lastEffectChange_.at=millis();
  lastEffectChange_.revision=frameRevision_;
  pattern_=pattern;
}

void LedGrid::stopPattern(const char *reason) {
  // Staging uses a separate buffer: abandon it without repainting unrelated
  // committed raw pixels from stale logical targets. The selected cells below
  // replace only their own pixels; no uncommitted stream bytes may leak out.
  if (staging_) { staging_ = false; dirty_ = true; }
  if (pattern_ == kPatternNone) return;
  changePattern(kPatternNone,reason);
  for (uint8_t i = 0; i < kCells; ++i) {
    current_[i] = target_[i];
    transitioning_[i] = false;
    renderCell(i);
  }
}

void LedGrid::renderCell(uint8_t index) {
  const uint8_t physical = physicalCells_[index];
  if (physical < 1 || physical > kCells) return;
  const uint16_t first = groupLayout_.start(physical);
  for (uint16_t sector = 0; sector < groupLayout_.count(physical); ++sector) {
    pixels_[first + sector] = current_[index];
  }
}

bool LedGrid::readyToShow(uint32_t now) const {
#if defined(ARDUINO_ARCH_ESP32)
  if(!output_.canAccept())return false;
#endif
  return kFrameIntervalMs == 0 ||
         static_cast<uint32_t>(now - lastFrameAt_) >= kFrameIntervalMs;
}

bool LedGrid::needsRender(uint32_t now) const {
  if (!outputEnabled_)return false;
  if (staging_) return static_cast<uint32_t>(now-stagingAt_)>=500U;
  if (!readyToShow(now)) return false;
  if(presentationAnimated())return true;
  if(diagnosticActive_ && diagnosticOptions_.animated() && !diagnosticOff_)return true;
  if(previewDuration_ && !previewActive(now))return true;
  if(PAKAR_TABLE_SELECTION && appearance_.enabled && !staticMode_ && pattern_==kPatternNone)return true;
  if(PAKAR_FIRST_WINS_ENABLED && keyEffects_ && keyWinner_ && !staticMode_)return true;
  if(pattern_==kPatternCellChase)return dirty_ ||
    (chaseStep_<kCells && static_cast<uint32_t>(now-chaseAt_)>=kChaseIntervalMs) ||
    static_cast<uint32_t>(now-lastFrameAt_)>=1000U;
  if (dirty_ || pattern_ == kPatternRainbow ||
      (pattern_ == kPatternCellChase && chaseStep_<kCells && static_cast<uint32_t>(now-chaseAt_)>=kChaseIntervalMs) ||
      (!staticMode_ && pattern_ != kPatternNone && pattern_ != kPatternCellChase)) return true;
  // A strip can power up after the ESP's first frame, or miss that frame.
  // Repair enabled static output without turning the fallback into animation.
  if (staticMode_) return static_cast<uint32_t>(now-lastFrameAt_)>=1000U;
  for (uint8_t i = 0; i < kCells; ++i) {
    if (current_[i] != target_[i]) return true;
  }
  // Refresh static output too; changed/animated frames retain their uncapped path.
  return static_cast<uint32_t>(now-lastFrameAt_)>=250U;
}

void LedGrid::service(uint32_t now) {
  if(!outputEnabled_)return;
  if(previewDuration_ && !previewActive(now))cancelPreview();
  if(staging_){if(static_cast<uint32_t>(now-stagingAt_)<500U)return;staging_=false;dirty_=true;}
  if (!readyToShow(now)) return;
  if(PAKAR_FIRST_WINS_ENABLED && keyEffects_ && keyWinner_ && !staticMode_){
    renderKeyEffects(now);showStaged();return;
  }
  if(pattern_==kPatternCellChase){
    if(chaseFirstFrame_){
      showStaged();chaseAt_=now;chaseFirstFrame_=false;return;
    }
    if(chaseStep_<chaseGroups() && static_cast<uint32_t>(now-chaseAt_)>=kChaseIntervalMs){
      // Never skip cells after a delayed loop, and never replay catch-up frames.
      ++chaseStep_;chaseAt_=now;renderChaseStep(chaseStep_);dirty_=true;
    }
    if(dirty_ || presentationAnimated() || (diagnosticActive_ && diagnosticOptions_.animated()) || static_cast<uint32_t>(now-lastFrameAt_)>=1000U)showStaged();
    return;
  }
  if (staticMode_ && !dirty_ && pattern_ != kPatternRainbow) {
    // Resend the already-published complete buffer, never advance fire/fades.
    if (presentationAnimated() || (diagnosticActive_ && diagnosticOptions_.animated()) || static_cast<uint32_t>(now-lastFrameAt_)>=1000U) showStaged();
    return;
  }
  const bool refresh=static_cast<uint32_t>(now-lastFrameAt_)>=250U;
  if (pattern_ != kPatternNone) {
    renderPattern(now);
    showStaged();
    return;
  }
  for (uint8_t i = 0; i < kCells; ++i) {
    const CRGB before = current_[i];
    if(transitioning_[i]){
      // Wall-clock interpolation, not one8-step jump per network/loop tick.
      // A delayed loop catches up to the same300ms transition without extending
      // its duration or exposing a partially updated selection.
      const uint32_t elapsed=static_cast<uint32_t>(now-transitionAt_[i]);
      const uint32_t duration=appearance_.enabled?appearance_.fadeMs:300U;
      const uint32_t progress=min<uint32_t>(elapsed,duration);
      const auto channel=[progress,duration](uint8_t from,uint8_t to){
        // Round the positive weighted sum once, identically in both directions.
        // Keep full wall-clock precision until the final8-bit wire value.
        return duration?static_cast<uint8_t>((uint32_t(from)*(duration-progress)+
          uint32_t(to)*progress+duration/2U)/duration):to;
      };
      current_[i]=CRGB(channel(transitionFrom_[i].r,target_[i].r),
                       channel(transitionFrom_[i].g,target_[i].g),
                       channel(transitionFrom_[i].b,target_[i].b));
      if(elapsed>=duration)transitioning_[i]=false;
    }
    if (current_[i] != before) {
      renderCell(i);
      dirty_ = true;
    }
  }
  if(PAKAR_TABLE_SELECTION && appearance_.enabled && !staticMode_){renderStateEffects(now);dirty_=true;}
  if (dirty_ || refresh || presentationAnimated() || (diagnosticActive_ && diagnosticOptions_.animated())) showStaged();
}

void LedGrid::renderStateEffects(uint32_t now) {
  for(uint8_t i=0;i<kCells;++i){
    const uint8_t physical=physicalCells_[i];if(!physical||physical>kCells)continue;
    const uint8_t count=groupLayout_.count(physical);
    const uint16_t first=groupLayout_.start(physical);
    const auto motion=appearance_.states[states_[i]].motion;
    const uint32_t elapsed=now-transitionAt_[i];
    CRGB base=current_[i];
    if(PAKAR_TABLE_SELECTION && appearance_.diagonal && states_[i]==BoardCore::Protocol::kCellIdle){
      // Same staggered 20-column geometry as clients; cable order is unrelated.
      const int x=2*(i%10)+(i/10)%2,y=2*(i/20)+(x%2);
      uint8_t highlight=0;
      for(uint8_t source=0;source<kCells;++source){
        const uint32_t age=now-transitionAt_[source];
        if(states_[source]!=BoardCore::Protocol::kCellLost || age>=600)continue;
        const int sx=2*(source%10)+(source/10)%2,sy=2*(source/20)+(sx%2);
        const int dx=x-sx,dy=y-sy;
        if((dx==1||dx==-1)&&(dy==1||dy==-1)){
          const uint32_t delay=dy<0?0:150;
          if(age>=delay && age<delay+450){
            const uint32_t phase=age-delay;
            const uint8_t gain=phase<150?uint8_t(phase*128/150):uint8_t((450-phase)*128/300);
            if(gain>highlight)highlight=gain;
          }
        }
      }
      const CRGB lost=colorFor(BoardCore::Protocol::kCellLost);
      base=CRGB((uint16_t(base.r)*(255-highlight)+uint16_t(lost.r)*highlight)/255,
        (uint16_t(base.g)*(255-highlight)+uint16_t(lost.g)*highlight)/255,
        (uint16_t(base.b)*(255-highlight)+uint16_t(lost.b)*highlight)/255);
    }
    for(uint16_t sector=0;sector<count;++sector){
      uint8_t gain=255;
      if(!transitioning_[i]){
        if(motion==BoardCore::KeyPresentation::Pulse)gain=BoardCore::StateMotion::pulse(elapsed,appearance_.pulseMs);
        else if(motion==BoardCore::KeyPresentation::Ring)gain=BoardCore::StateMotion::ring(elapsed,appearance_.ringMs,sector,count,appearance_.neighbors,appearance_.invert);
        else if(motion==BoardCore::KeyPresentation::SharedChase)gain=BoardCore::StateMotion::chase(elapsed,appearance_.chaseMs,sector,count);
      }
      pixels_[first+sector]=CRGB(uint16_t(base.r)*gain/255,uint16_t(base.g)*gain/255,uint16_t(base.b)*gain/255);
    }
  }
}

void LedGrid::renderKeyEffects(uint32_t now) {
  const uint32_t elapsed=now-keyWinnerAt_;
  const auto &p=keyPresentation_;
  uint16_t lostPixels=0;
  for(uint8_t box=0;box<9;++box)if((keyMask_&(1U<<box))&&box+1!=keyWinner_&&p.groups[box*2+1].loseMotion==BoardCore::KeyPresentation::SharedChase)
    lostPixels+=groupLayout_.count(physicalCells_[box*2+1]);
  uint16_t lostOffset=0;
  for(uint8_t group=0;group<kCells;++group){
    const uint8_t physical=physicalCells_[group];
    const uint8_t count=groupLayout_.count(physical);
    const uint16_t first=groupLayout_.start(physical);
    const bool won=group/2+1==keyWinner_;
    const bool enabled=keyMask_&(1U<<(group/2));
    const auto &style=p.groups[group];
    const uint8_t motion=won?style.winMotion:style.loseMotion;
    const CRGB destination=target_[group];
    const CRGB idle(keyIdle_>>16,keyIdle_>>8,keyIdle_);
    const uint32_t fade=min(elapsed,uint32_t(p.fadeMs));
    const CRGB base=(!won&&p.fadeMs)?CRGB(
      (uint32_t(idle.r)*(p.fadeMs-fade)+uint32_t(destination.r)*fade)/p.fadeMs,
      (uint32_t(idle.g)*(p.fadeMs-fade)+uint32_t(destination.g)*fade)/p.fadeMs,
      (uint32_t(idle.b)*(p.fadeMs-fade)+uint32_t(destination.b)*fade)/p.fadeMs):destination;
    for(uint16_t sector=0;sector<count;++sector){
      uint8_t gain=255;
      if(won&&group%2==0&&elapsed<p.flashDuration())gain=p.flashLit(elapsed)?255:0;
      else if(!won&&elapsed<p.fadeMs){
        // Reach the pure losing color before starting its decorative motion.
        // Otherwise pulse/chase gain dims the white-to-red transition itself.
        gain=255;
      }
      else if(motion==BoardCore::KeyPresentation::Pulse){
        // Indicator group: a uniform triangular pulse; no moving pixels.
        gain=BoardCore::StateMotion::pulse(won?elapsed:elapsed-p.fadeMs,p.pulseMs);
      }else if(motion==BoardCore::KeyPresentation::SharedChase&&!won&&group%2&&lostPixels){
        gain=BoardCore::StateMotion::chase(elapsed,p.chaseMs,lostOffset+sector,lostPixels);
      }else if(motion==BoardCore::KeyPresentation::Ring){
        // Illumination group: one travelling dot with optional immediate
        // neighbours. Keep hue pure and calculate from monotonic time.
        gain=BoardCore::StateMotion::ring(elapsed,keyChasePeriodMs_,sector,
          count,keyChaseNeighbors_,keyChaseInvert_);
      }
      if(!enabled)gain=0;
      pixels_[first+sector]=CRGB(
        uint16_t(base.r)*gain/255U,uint16_t(base.g)*gain/255U,uint16_t(base.b)*gain/255U);
    }
    if(enabled&&!won&&group%2&&motion==BoardCore::KeyPresentation::SharedChase)lostOffset+=count;
  }
}

void LedGrid::renderPattern(uint32_t now) {
  switch (pattern_) {
    case kPatternRainbow: {
      // Like the donor's monotonic output timeline, skip obsolete animation
      // positions after a slow service rather than stretching its cadence.
      // This sets speed (3 hue units/16ms), not a render/FPS ceiling. Unsigned
      // subtraction crosses millis() rollover; widen before multiplying so
      // long-running effects cannot overflow before modulo-256 hue conversion.
      const uint32_t elapsed=static_cast<uint32_t>(now-rainbowStartedAt_);
      hue_=static_cast<uint8_t>(rainbowBaseHue_+(uint64_t(elapsed)*3U)/16U);
      fill_rainbow(pixels_, kPixels, hue_, 7);
      break;
    }
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

void LedGrid::renderChaseStep(uint8_t step) {
  chaseCell_=kCells-step+1;
#if PAKAR_FIRST_WINS_ENABLED
  uint8_t ordinal=0;
  for(uint8_t group=kCells;group;--group)if(keyMask_&(1U<<((group-1)/2))){
    if(++ordinal==step){chaseCell_=group;break;}
  }
#endif
  // Compose both the previous white cell and new red cell before one show.
  // Follow the active board mapping, never a client-side guessed permutation.
  for(uint8_t i=0;i<kCells;++i){
    const bool selected=i+1==chaseCell_;
    states_[i]=selected?BoardCore::Protocol::kCellRed:BoardCore::Protocol::kCellWhite;
    current_[i]=target_[i]=selected?CRGB::Red:CRGB::White;
    transitioning_[i]=false;renderCell(i);
  }
}

void LedGrid::setBrightness(uint8_t value) {
  brightness_ = value;
#if !defined(ARDUINO_ARCH_ESP32)
  FastLED.setBrightness(value);
#endif
  dirty_ = true;
}

BoardCore::Protocol::CellState LedGrid::state(uint8_t cell) const {
  if (cell < 1 || cell > kCells) return BoardCore::Protocol::kCellOff;
  return static_cast<BoardCore::Protocol::CellState>(states_[cell - 1]);
}

void LedGrid::setPhysicalMapping(const uint8_t *physicalCells, uint8_t count) {
  if (physicalCells == nullptr || count != kCells) return;
  bool seen[kCells] = {};
  for (uint8_t i = 0; i < kCells; ++i) {
    const uint8_t physical = physicalCells[i];
    if (physical < 1 || physical > kCells || seen[physical - 1]) return;
    seen[physical - 1] = true;
  }
  clearDiagnostic();
  fill_solid(pixels_, kPixels, CRGB::Black);
  staging_ = false;
  for (uint8_t i = 0; i < kCells; ++i) physicalCells_[i] = physicalCells[i];
  if(pattern_==kPatternCellChase)renderChaseStep(chaseStep_);
  for (uint8_t i = 0; i < kCells; ++i) renderCell(i);
  dirty_ = true;
}

uint8_t LedGrid::physicalCell(uint8_t logicalCell) const {
  if (logicalCell < 1 || logicalCell > kCells) return 0;
  return physicalCells_[logicalCell - 1];
}

bool LedGrid::configureGroups(const GroupLayout &layout) {
  if(!canConfigureGroups())return false;
  groupLayout_=layout;
  // Recompose the existing semantic scene atomically; never reinterpret old
  // raw stream bytes under new coordinates or leave the shortened tail lit.
  fill_solid(pixels_,kPixels,CRGB::Black);
  for(uint8_t i=0;i<kCells;++i)renderCell(i);
  dirty_=true;return true;
}

bool LedGrid::canStage(uint64_t owner) {
  if(staging_ && static_cast<uint32_t>(millis()-stagingAt_)>=500U){staging_=false;dirty_=true;}
  return !staging_ || stageOwner_==owner;
}

bool LedGrid::commitStaged(uint64_t owner) {
  if(staging_ && static_cast<uint32_t>(millis()-stagingAt_)>=500U){staging_=false;dirty_=true;return false;}
  if(staging_ && stageOwner_!=owner)return false;
  // Raw streaming supersedes pending cell fades, without painting their targets
  // over the committed raw frame on the next renderer tick.
  if(staging_)for(uint8_t i=0;i<kCells;++i){current_[i]=target_[i];transitioning_[i]=false;}
  showStaged();
  return true;
}

bool LedGrid::diagnostic(uint16_t index,uint8_t count,uint32_t rgb,bool off,const BoardCore::PixelDiagnostic &options) {
  // Reject completely before touching an in-progress stream or renderer.
  if(!groupLayout_.diagnosticRange(index,count) ||
     rgb>0xffffffU || !options.valid() || !canStage(UINT64_MAX))return false;
  if(options.enabled)retirePresentation();
  cancelPreview();
  if(!options.enabled){
    if(!diagnosticActive_)++diagnosticRevision_;
    clearDiagnostic();diagnosticOptions_=options;
    diagnosticPixel_=index+1;diagnosticCount_=count;diagnosticRgb_=rgb;diagnosticOff_=off;
    // Stopping an overlay restores the complete underlying frame immediately,
    // including the original disabled-output policy.
    if(outputEnabled_)showStaged();else {
      ++frameRevision_;setOutputEnabled(false);
      fill_solid(published_,kPixels,CRGB::Black);
      ::memcpy(publishedPhysical_,physicalCells_,sizeof(publishedPhysical_));
      publishedGroups_=groupLayout_;
      publishedBrightness_=brightness_;lastFrameAt_=millis();dirty_=false;
    }
    return true;
  }
  if(!diagnosticActive_)diagnosticOutputBefore_=outputEnabled_;
  diagnosticActive_=true;diagnosticPixel_=index+1;diagnosticAt_=millis();
  diagnosticOptions_=options;outputEnabled_=true;
  diagnosticCount_=count;diagnosticRgb_=rgb;diagnosticOff_=off;++diagnosticRevision_;
  showStaged(); // Complete publication; saved scene/pattern/winner remain intact.
  if(panelObserver_)panelObserver_(4,index+1,off?0:count);
  return true;
}

bool LedGrid::stagePixels(uint16_t offset, const uint8_t *rgb, uint8_t count,uint64_t owner) {
  if (offset + count > kPixels || rgb == nullptr) return false;
  if(!canStage(owner))return false;
  retirePresentation();
  cancelPreview();
  keyWinner_=0;
  if(owner!=UINT64_MAX)clearDiagnostic();
  if(!staging_)::memcpy(staged_,pixels_,sizeof(staged_));
  staging_ = true;
  stageOwner_=owner;
  stagingAt_=millis();
  changePattern(kPatternNone,"raw-stream");
  for (uint8_t i = 0; i < count; ++i) {
    staged_[offset + i] = CRGB(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
  }
  return true;
}

void LedGrid::showStaged() {
  if(staging_)::memcpy(pixels_,staged_,sizeof(pixels_));
  const bool diagnosing=diagnosticActive_ && !staging_ && !maintenanceActive_;
  const bool previewing=previewActive(millis()) && !staging_ && !maintenanceActive_ && !diagnosing;
  const bool presenting=presentation_&&presentation_->active()&&!staging_&&!maintenanceActive_;
  if(diagnosing||presenting)::memcpy(staged_,pixels_,sizeof(pixels_));
  if(previewing){
    // Reuse the unowned staging workspace; never commit a preview to the base.
    ::memcpy(staged_,pixels_,sizeof(pixels_));
    const CRGB color(previewRgb_>>16,previewRgb_>>8,previewRgb_);
    for(uint8_t id=1;id<=kCells;++id)if(previewSelection_.contains(id)){
      const uint8_t physical=physicalCells_[id-1];if(!physical||physical>kCells)continue;
      for(uint16_t sector=0;sector<groupLayout_.count(physical);++sector)pixels_[groupLayout_.start(physical)+sector]=color;
    }
  }
  // In the fallback every physical group is one indivisible cell. The first
  // pixel supplies its color, including explicit rainbow and streamed input.
  if(staticMode_)for(uint8_t group=1;group<=kCells;++group){
    const uint16_t first=groupLayout_.start(group);
    for(uint16_t sector=1;sector<groupLayout_.count(group);++sector)pixels_[first+sector]=pixels_[first];
  }
  if(presenting)presentation_->compose(pixels_,groupLayout_,physicalCells_,millis());
#if PAKAR_FIRST_WINS_ENABLED
  // Clear disabled physical boxes on EVERY frame, including rainbow/stream.
  // Retain the black tail on wire so resizing cannot leave stale LEDs lit.
  for(uint8_t logical=0;logical<kCells;++logical)if(!(keyMask_&(1U<<(logical/2)))){
    const uint8_t group=physicalCells_[logical];
    for(uint16_t sector=0;sector<groupLayout_.count(group);++sector)pixels_[groupLayout_.start(group)+sector]=CRGB::Black;
  }
#endif
  if(diagnosing){
    if(diagnosticOptions_.composite==BoardCore::PixelDiagnostic::Clear)fill_solid(pixels_,kPixels,CRGB::Black);
    CRGB color(diagnosticRgb_>>16,(diagnosticRgb_>>8)&255,diagnosticRgb_&255);
    const uint32_t elapsed=uint32_t(millis()-diagnosticAt_);
    if(diagnosticOptions_.effect==BoardCore::PixelDiagnostic::Rainbow)
      fill_rainbow(&color,1,uint8_t(uint64_t(elapsed%diagnosticOptions_.periodMs)*256U/diagnosticOptions_.periodMs),0);
    const uint8_t gain=diagnosticOff_?0:diagnosticOptions_.gain(elapsed);
    color=CRGB((uint16_t(color.r)*gain+127U)/255U,(uint16_t(color.g)*gain+127U)/255U,(uint16_t(color.b)*gain+127U)/255U);
    for(uint16_t i=0;i<diagnosticCount_;++i)pixels_[diagnosticPixel_-1+i]=color;
  }
  for(uint16_t i=groupLayout_.used();i<kPixels;++i)pixels_[i]=CRGB::Black;
#if defined(ARDUINO_ARCH_ESP32)
  if(outputEnabled_)output_.submit(corrected(pixels_),brightness_,true,frameRevision_+1);
#else
  if(outputEnabled_){
  FastLED.show();
  }
#endif
  ::memcpy(published_,pixels_,sizeof(published_));
  if(previewing || diagnosing || presenting)::memcpy(pixels_,staged_,sizeof(pixels_));
  publishedPresentationRevision_=presentation_?presentation_->revision:0;
  ::memcpy(publishedPhysical_,physicalCells_,sizeof(publishedPhysical_));
  publishedGroups_=groupLayout_;
  publishedBrightness_=brightness_;
  staging_ = false;
  ++frameRevision_;
  lastFrameAt_ = millis();
  dirty_ = false;
}

void LedGrid::setOutputEnabled(bool enabled){
  if(!enabled)retirePresentation();
  // Explicit Off always queues a complete black frame, even if our requested
  // state was already Off: WS2811 retains previously latched (possibly corrupt)
  // values until it receives a valid replacement frame.
  if(outputEnabled_==enabled && enabled && !diagnosticActive_)return;
  clearDiagnostic();
  outputEnabled_=enabled;
  if(!enabled){clearDiagnostic();cancelPreview();}
  if(panelObserver_)panelObserver_(9,enabled,0);
  if(enabled && pattern_==kPatternCellChase)chaseAt_=millis();
#if defined(ARDUINO_ARCH_ESP32)
  output_.submit(corrected(pixels_),brightness_,enabled,frameRevision_);
#else
  if(!enabled){CRGB saved[kPixels];::memcpy(saved,pixels_,sizeof(pixels_));fill_solid(pixels_,kPixels,CRGB::Black);FastLED.show();::memcpy(pixels_,saved,sizeof(pixels_));}
#endif
  dirty_=true;
}
void LedGrid::setStaticMode(bool enabled){
  if(staticMode_==enabled)return;
  staticMode_=enabled;
  staging_=false;
  if(enabled){
    clearDiagnostic();
    for(uint8_t i=0;i<kCells;++i){current_[i]=target_[i];transitioning_[i]=false;renderCell(i);}
  }
  dirty_=true;
}
String LedGrid::outputCapabilitiesJson()const{
#if defined(ARDUINO_ARCH_ESP32)
  return output_.capabilitiesJson();
#else
  return F("{\"backend\":\"fastled\",\"rmt\":false,\"dma\":false}");
#endif
}
String LedGrid::outputJson(bool compact)const{
#if !defined(ARDUINO_ARCH_ESP32)
  (void)compact;
#endif
  String value=F("{\"enabled\":");value+=outputEnabled_?F("true"):F("false");
  value+=F(",\"mode\":\"");value+=staticMode_?F("static"):F("effects");value+='"';
#if defined(ARDUINO_ARCH_ESP32)
  value+=F(",\"backend_saved\":");value+=backendSaved_?F("true"):F("false");
  value+=F(",\"backend_settings_error\":\"");value+=backendSettingsError_;value+='"';
#endif
  // Small authoritative observation on normal REST/WS/USB status, not a full
  // 480-pixel poll. Revision changes only on diagnostic intent/invalidation.
  value+=F(",\"diagnostic\":{\"revision\":");value+=diagnosticRevision_;
  value+=F(",\"group_layout_revision\":");value+=groupLayout_.revision();
  value+=F(",\"active\":");value+=diagnosticActive_?F("true"):F("false");
  value+=F(",\"index\":");value+=uint32_t(diagnosticPixel_?diagnosticPixel_-1:0);
  value+=F(",\"count\":");value+=diagnosticCount_;
  value+=F(",\"rgb\":");value+=diagnosticRgb_;
  value+=F(",\"off\":");value+=diagnosticOff_?F("true"):F("false");
  value+=F(",\"enabled\":");value+=diagnosticActive_?F("true"):F("false");
  value+=F(",\"effect\":\"");value+=BoardCore::PixelDiagnostic::name(diagnosticOptions_.effect);
  value+=F("\",\"composite\":\"");value+=BoardCore::PixelDiagnostic::name(diagnosticOptions_.composite);value+='"';
  value+=F(",\"period_ms\":");value+=uint32_t(diagnosticOptions_.periodMs);
  value+=F(",\"duty_percent\":");value+=diagnosticOptions_.dutyPercent;
  value+=F(",\"min_brightness\":");value+=diagnosticOptions_.minBrightness;
  value+=F(",\"max_brightness\":");value+=diagnosticOptions_.maxBrightness;value+='}';
  value+=F(",\"refresh_interval_ms\":");value+=staticMode_?1000U:250U;
  value+=F(",\"cell_chase\":{\"active\":");value+=(pattern_==kPatternCellChase&&chaseStep_<kCells)?F("true"):F("false");
  value+=F(",\"physical_cell\":");value+=physicalCell(chaseCell_);
  value+=F(",\"visited\":");value+=chaseStep_;
  value+=F(",\"logical_cell\":");value+=chaseCell_;
  value+=F(",\"interval_ms\":");value+=kChaseIntervalMs;
  value+=F(",\"complete\":");value+=(pattern_==kPatternCellChase&&chaseStep_==kCells)?F("true"):F("false");value+='}';
#if defined(ARDUINO_ARCH_ESP32)
  value+=F(",\"driver\":");value+=output_.json(compact);
#else
  value+=F(",\"driver\":");value+=outputCapabilitiesJson();
#endif
  if(lastEffectChange_.reason){
    value+=F(",\"last_effect_change\":{\"reason\":\"");value+=lastEffectChange_.reason;
    value+=F("\",\"previous\":\"");value+=effectName(lastEffectChange_.previous);
    value+=F("\",\"new_pattern\":\"");value+=effectName(lastEffectChange_.next);
    value+=F("\",\"millis\":");value+=lastEffectChange_.at;
    value+=F(",\"revision\":");value+=lastEffectChange_.revision;value+='}';
  }
  value+='}';return value;
}

String LedGrid::pixelFrameJson(bool compact) const {
  if (!frameRevision_) return F("null");
  String body;
  if (!body.reserve((compact ? 0U : kPixels * 6U + kCells * 4U) + kCells*10U + 200U)) return F("null");
  body = F("{\"revision\":"); body += frameRevision_;
  body += F(",\"brightness\":"); body += publishedBrightness_;
  body += F(",\"pending\":"); body += dirty_||staging_?F("true"):F("false");
  body += F(",\"group_layout_revision\":");body+=publishedGroups_.revision();
  body += F(",\"presentation_revision\":");body+=publishedPresentationRevision_;
  body += F(",\"group_counts\":[");
  for(uint8_t group=1;group<=kCells;++group){if(group>1)body+=',';body+=publishedGroups_.count(group);}
  body += F("],\"group_starts\":[");
  for(uint8_t group=1;group<=kCells;++group){if(group>1)body+=',';body+=uint32_t(publishedGroups_.start(group));}
  body += ']';
  if(compact){body+='}';return body;}
  body += F(",\"rgb\":\"");
  const char hex[] = "0123456789abcdef";
  for (uint16_t i=0;i<kPixels;++i) {
    const uint8_t channels[] = {published_[i].r,published_[i].g,published_[i].b};
    for (uint8_t channel : channels) { body += hex[channel >> 4]; body += hex[channel & 15]; }
  }
  body += F("\",\"physical\":[");
  for (uint8_t i=0;i<kCells;++i) { if(i)body+=',';body+=publishedPhysical_[i]; }
  body += F("]}");
  return body;
}

void LedGrid::presentMaintenance(const PakarUpdate::Lifecycle &life,uint32_t now) {
  using V=PakarUpdate::Visual;
  const auto view=life.visual(now);
  if(view==V::None || !outputEnabled_) {
    if(maintenanceActive_) {maintenanceActive_=maintenanceBlack_=false;maintenanceVisual_=V::None;dirty_=true;}
    return;
  }
  if(!readyToShow(now) || (maintenanceActive_ && uint32_t(now-lastFrameAt_)<25U))return;
  const uint8_t alpha=life.sceneAlpha(now);
  const bool black=view==V::Shutdown && !alpha;
  if(black && maintenanceBlack_)return; // Retain the exact terminal wire receipt.
  clearDiagnostic();
  staging_=false; // Never resume a partial network frame after maintenance.
  if(view==V::Shutdown && maintenanceVisual_!=view)
    ::memcpy(maintenanceFrom_,published_,sizeof(maintenanceFrom_));
  for(uint8_t cell=0;cell<kCells;++cell) {
    const uint8_t physical=physicalCells_[cell];
    if(physical<1 || physical>kCells)continue;
    const uint16_t first=groupLayout_.start(physical);
    const uint32_t rgb=life.cellRgb(cell,now);
    const CRGB accent(rgb>>16,(rgb>>8)&255,rgb&255);
    for(uint16_t pixel=0;pixel<groupLayout_.count(physical);++pixel) {
      const CRGB scene=view==V::Shutdown?maintenanceFrom_[first+pixel]:pixels_[first+pixel];
      const auto mix=[alpha](uint8_t a,uint8_t b){return uint8_t((uint16_t(a)*(255-alpha)+uint16_t(b)*alpha+127)/255);};
      published_[first+pixel]=(view==V::Startup || view==V::Shutdown)?
        CRGB(mix(0,scene.r),mix(0,scene.g),mix(0,scene.b)):
        (view==V::Success || view==V::Failure)?
        CRGB(mix(accent.r,scene.r),mix(accent.g,scene.g),mix(accent.b,scene.b)):accent;
    }
  }
  for(uint16_t i=groupLayout_.used();i<kPixels;++i)published_[i]=CRGB::Black;
#if defined(ARDUINO_ARCH_ESP32)
  output_.submit(corrected(published_),brightness_,true,frameRevision_+1);
#else
  // FastLED owns pixels_' address. Stage/show/restore without another frame RAM.
  ::memcpy(staged_,pixels_,sizeof(pixels_));::memcpy(pixels_,published_,sizeof(pixels_));
  FastLED.show();::memcpy(pixels_,staged_,sizeof(pixels_));
#endif
  ::memcpy(publishedPhysical_,physicalCells_,sizeof(physicalCells_));
  publishedGroups_=groupLayout_;
  publishedBrightness_=brightness_;++frameRevision_;lastFrameAt_=now;
  maintenanceActive_=true;maintenanceBlack_=black;
  maintenanceVisual_=view;
}
bool LedGrid::maintenancePresented()const {
  if(!outputEnabled_)return true;
#if defined(ARDUINO_ARCH_ESP32)
  return maintenanceBlack_ && output_.presented(frameRevision_);
#else
  return maintenanceBlack_;
#endif
}
