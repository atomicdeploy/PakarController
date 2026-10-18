#include "LedGrid.h"
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
  output_.begin();
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

CRGB LedGrid::colorFor(BoardCore::Protocol::CellState state) {
  switch (state) {
    case BoardCore::Protocol::kCellRed: return CRGB::Red;
    case BoardCore::Protocol::kCellOff: return CRGB::Black;
    default: return CRGB::White;
  }
}

bool LedGrid::setCell(uint8_t cell, BoardCore::Protocol::CellState state,
                      BoardCore::Protocol::Effect effect) {
  if(cell<1||cell>kCells)return false;
  BoardCore::CellBatch selection;selection.add(cell);return setCells(selection,state,effect);
}

bool LedGrid::setCells(const BoardCore::CellBatch &selection,
                      BoardCore::Protocol::CellState state,BoardCore::Protocol::Effect effect) {
  if(!selection.count()||state>BoardCore::Protocol::kCellOff||effect>BoardCore::Protocol::kEffectPulse)return false;
  // Read actual last-published logical RGB before stopping a global pattern.
  // No yields, persistence, UART fan-out or output submission inside this loop.
  for(uint8_t id=1;id<=kCells;++id)if(selection.contains(id)){
    const uint8_t index=id-1,physical=publishedPhysical_[index];
    transitionFrom_[index]=frameRevision_&&physical>=1&&physical<=kCells?
      published_[(physical-1)*kSectorsPerCell]:current_[index];
  }
  stopPattern("cell-batch");
  const uint32_t started=millis();
  for(uint8_t id=1;id<=kCells;++id)if(selection.contains(id)){
    const uint8_t index=id-1;states_[index]=state;target_[index]=colorFor(state);
    transitioning_[index]=!staticMode_&&effect!=BoardCore::Protocol::kEffectImmediate;
    if(transitioning_[index]){
      if(effect==BoardCore::Protocol::kEffectPulse)transitionFrom_[index]=CRGB::Black;
      current_[index]=transitionFrom_[index];transitionAt_[index]=started;
    }else current_[index]=target_[index];
    renderCell(index);
  }
  dirty_=true;return true;
}

void LedGrid::fill(BoardCore::Protocol::CellState state) {
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
}

void LedGrid::resetWhite() {
  changePattern(kPatternNone,"reset-white");
  fill(BoardCore::Protocol::kCellWhite);
}

void LedGrid::showWinner(uint8_t winner) {
  if (winner < 1 || winner > 9) return;
  staging_ = false;
  changePattern(kPatternNone,"winner");
  for (uint8_t i = 0; i < kCells; ++i) {
    const uint8_t group = static_cast<uint8_t>((static_cast<uint16_t>(i) * 9U) /
                                               kCells) + 1U;
    states_[i] = group == winner ? BoardCore::Protocol::kCellRed
                                 : BoardCore::Protocol::kCellOff;
    target_[i] = colorFor(static_cast<BoardCore::Protocol::CellState>(states_[i]));
    current_[i] = target_[i];
    transitioning_[i] = false;
    renderCell(i);
  }
  dirty_ = true;
}

void LedGrid::setPattern(Pattern pattern) {
  if (pattern == kPatternNone) {
    stopPattern("explicit-none");
    dirty_ = true;
    return;
  }
  staging_ = false;
  changePattern(pattern,"explicit-pattern");
  if(pattern==kPatternCellChase){
    chaseStep_=1;chaseFirstFrame_=true;chaseAt_=millis();renderChaseStep(chaseStep_);
  }
  for(uint8_t i=0;i<kCells;++i)transitioning_[i]=false;
  dirty_ = true;
}

const char *LedGrid::patternName() const {
  return effectName(pattern_);
}

void LedGrid::changePattern(Pattern pattern,const char *reason) {
  if(pattern_==pattern)return;
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
  const uint16_t first = static_cast<uint16_t>(physical - 1) * kSectorsPerCell;
  for (uint8_t sector = 0; sector < kSectorsPerCell; ++sector) {
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
  if(staging_){if(static_cast<uint32_t>(now-stagingAt_)<500U)return;staging_=false;dirty_=true;}
  if (!readyToShow(now)) return;
  if(pattern_==kPatternCellChase){
    if(chaseFirstFrame_){
      showStaged();chaseAt_=now;chaseFirstFrame_=false;return;
    }
    if(chaseStep_<kCells && static_cast<uint32_t>(now-chaseAt_)>=kChaseIntervalMs){
      // Never skip cells after a delayed loop, and never replay catch-up frames.
      ++chaseStep_;chaseAt_=now;renderChaseStep(chaseStep_);dirty_=true;
    }
    if(dirty_ || static_cast<uint32_t>(now-lastFrameAt_)>=1000U)showStaged();
    return;
  }
  if (staticMode_ && !dirty_ && pattern_ != kPatternRainbow) {
    // Resend the already-published complete buffer, never advance fire/fades.
    if (static_cast<uint32_t>(now-lastFrameAt_)>=1000U) showStaged();
    return;
  }
  const bool refresh=static_cast<uint32_t>(now-lastFrameAt_)>=250U;
  if (pattern_ != kPatternNone) {
    renderPattern();
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
      const uint32_t progress=min<uint32_t>(elapsed,300U);
      const auto channel=[progress](uint8_t from,uint8_t to){
        return static_cast<uint8_t>(int32_t(from)+(int32_t(to)-from)*int32_t(progress)/300);
      };
      current_[i]=CRGB(channel(transitionFrom_[i].r,target_[i].r),
                       channel(transitionFrom_[i].g,target_[i].g),
                       channel(transitionFrom_[i].b,target_[i].b));
      if(elapsed>=300U)transitioning_[i]=false;
    }
    if (current_[i] != before) {
      renderCell(i);
      dirty_ = true;
    }
  }
  if (dirty_ || refresh) showStaged();
}

void LedGrid::renderPattern() {
  if (!staticMode_ || pattern_ == kPatternRainbow) hue_ = static_cast<uint8_t>(hue_ + 3U);
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

void LedGrid::renderChaseStep(uint8_t step) {
  chaseCell_=kCells-step+1;
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

bool LedGrid::diagnostic(uint16_t index,uint8_t count,uint32_t rgb,bool off) {
  // Reject completely before touching an in-progress stream or renderer.
  if(index>=kPixels || (count!=1 && count!=kSectorsPerCell) ||
     index+count>kPixels || (count==kSectorsPerCell && index%kSectorsPerCell) ||
     rgb>0xffffffU || !canStage(UINT64_MAX))return false;
  changePattern(kPatternNone,"pixel-diagnostic");
  staticMode_=false; // Single pixels must not be replicated by grouped fallback.
  fill_solid(staged_,kPixels,CRGB::Black);
  if(!off)for(uint8_t i=0;i<count;++i)
    staged_[index+i]=CRGB(rgb>>16,(rgb>>8)&255,rgb&255);
  for(uint8_t i=0;i<kCells;++i){current_[i]=target_[i];transitioning_[i]=false;}
  staging_=true;stageOwner_=UINT64_MAX;stagingAt_=millis();
  outputEnabled_=true;
  showStaged(); // Includes the black tail and exactly one immutable publication.
  return true;
}

bool LedGrid::stagePixels(uint16_t offset, const uint8_t *rgb, uint8_t count,uint64_t owner) {
  if (offset + count > kPixels || rgb == nullptr) return false;
  if(!canStage(owner))return false;
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
  // In the fallback every physical group is one indivisible cell. The first
  // pixel supplies its color, including explicit rainbow and streamed input.
  if(staticMode_)for(uint16_t first=0;first<kPixels;first+=kSectorsPerCell)
    for(uint8_t sector=1;sector<kSectorsPerCell;++sector)pixels_[first+sector]=pixels_[first];
#if defined(ARDUINO_ARCH_ESP32)
  if(outputEnabled_)output_.submit(pixels_,brightness_,true,frameRevision_+1);
#else
  if(outputEnabled_){
  FastLED.show();
  }
#endif
  ::memcpy(published_,pixels_,sizeof(published_));
  ::memcpy(publishedPhysical_,physicalCells_,sizeof(publishedPhysical_));
  publishedBrightness_=brightness_;
  staging_ = false;
  ++frameRevision_;
  lastFrameAt_ = millis();
  dirty_ = false;
}

void LedGrid::setOutputEnabled(bool enabled){
  // Explicit Off always queues a complete black frame, even if our requested
  // state was already Off: WS2811 retains previously latched (possibly corrupt)
  // values until it receives a valid replacement frame.
  if(outputEnabled_==enabled && enabled)return;
  outputEnabled_=enabled;
  if(enabled && pattern_==kPatternCellChase)chaseAt_=millis();
#if defined(ARDUINO_ARCH_ESP32)
  output_.submit(pixels_,brightness_,enabled,frameRevision_);
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
String LedGrid::outputJson()const{
  String value=F("{\"enabled\":");value+=outputEnabled_?F("true"):F("false");
  value+=F(",\"mode\":\"");value+=staticMode_?F("static"):F("effects");value+='"';
  value+=F(",\"refresh_interval_ms\":");value+=staticMode_?1000U:250U;
  value+=F(",\"cell_chase\":{\"active\":");value+=(pattern_==kPatternCellChase&&chaseStep_<kCells)?F("true"):F("false");
  value+=F(",\"physical_cell\":");value+=physicalCell(chaseCell_);
  value+=F(",\"visited\":");value+=chaseStep_;
  value+=F(",\"logical_cell\":");value+=chaseCell_;
  value+=F(",\"interval_ms\":");value+=kChaseIntervalMs;
  value+=F(",\"complete\":");value+=(pattern_==kPatternCellChase&&chaseStep_==kCells)?F("true"):F("false");value+='}';
#if defined(ARDUINO_ARCH_ESP32)
  value+=F(",\"driver\":");value+=output_.json();
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

String LedGrid::pixelFrameJson() const {
  if (!frameRevision_) return F("null");
  String body;
  if (!body.reserve(kPixels * 6U + kCells * 4U + 120U)) return F("null");
  body = F("{\"revision\":"); body += frameRevision_;
  body += F(",\"brightness\":"); body += publishedBrightness_;
  body += F(",\"pending\":"); body += dirty_||staging_?F("true"):F("false");
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
