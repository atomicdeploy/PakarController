#include "FrontPanel.h"

#include "SevenSegments.h"

#include <string.h>

namespace {
constexpr uint8_t kRepeatMask = 0x03U;
constexpr uint8_t kRawCells = 0x20U;
constexpr uint8_t kIntervalWaiting = 0x40U;
constexpr uint8_t kForceScroll = 0x80U;
constexpr uint8_t kDisplaySegments = 0;
constexpr uint8_t kDisplayReleaseHostPanel = 4;
constexpr uint8_t kDisplayScheduledSegments = 5;
constexpr uint8_t kSegmentA = 0x01;
constexpr uint8_t kSegmentB = 0x02;
constexpr uint8_t kSegmentC = 0x04;
constexpr uint8_t kSegmentD = 0x08;
constexpr uint8_t kSegmentE = 0x10;
constexpr uint8_t kSegmentF = 0x20;

uint16_t readU16(const uint8_t *value) {
  return static_cast<uint16_t>(value[0] | value[1] << 8);
}
}  // namespace

FrontPanel frontPanel;

void FrontPanel::setEspMenu(bool active, uint8_t page, uint8_t notice,
                             uint8_t value, const char *hostname, uint32_t now) {
  if (active && (!espActive_ || page != espPage_)) espPageAt_ = now;
  espActive_ = active; espPage_ = page; espNotice_ = notice;
  espValue_ = value; espHostname_ = hostname;
}

void FrontPanel::showEspMenu(uint32_t now) {
  using namespace BoardCore::Protocol;
  if (espNotice_ == EspPanel::Pending) { display.showText(F("buSY")); return; }
  if (espNotice_ == EspPanel::Failed) { display.showText(F("Err ")); return; }
  if (espNotice_ == EspPanel::TimedOut) { display.showText(F("tout")); return; }
  if (espNotice_ == EspPanel::Saved) {
    display.showText(espPage_ == 1 || espPage_ == 2 ? F("SEnt") : F("donE"));
    return; // Scan/reconnect ACK means queued, never completed scan/link success.
  }
  const uint32_t elapsed = now - espPageAt_;
  if (elapsed < 650) {
    switch (espPage_) {
      case 0: display.showText(F("nEt ")); break;
      case 1: display.showText(F("rECn")); break;
      case 2: display.showText(F("SCAn")); break;
      case 3: display.showText(F("dHCP")); break;
      case 4: display.showText(F("nAME")); break;
      default: display.showText(F("bAC ")); break;
    }
    return;
  }
  if (espPage_ == 0) {
    display.showText(espValue_ == 1 ? F("AP  ") : espValue_ == 2 ? F("CL  ") : F("both"));
  } else if (espPage_ == 3) display.showText(espValue_ ? F("Auto") : F("StAt"));
  else if (espPage_ == 4 && espHostname_) {
    // One pass only; do not resume the annoying perpetual default marquee.
    const uint8_t length = strlen(espHostname_);
    const uint32_t step = (elapsed - 650) / 350;
    const uint8_t offset = length > 4 && step < static_cast<uint32_t>(length - 3) ? step
        : length > 4 ? length - 4 : 0;
    char text[5] = {};
    for (uint8_t i = 0; i < 4 && offset + i < length; ++i)
      text[i] = espHostname_[offset + i];
    display.showText(text);
  } else display.showText(F("SEL "));
}

void FrontPanel::begin(uint32_t now) {
  animationStartedAt_ = now;
  animationActive_ = true;
  lastRefreshAt_ = now - 20U;
}

void FrontPanel::release() {
  hostTextActive_ = false;
  transientActive_ = false;
  options_ = 0;
  captured_ = false;
  capturedMeta_ = 0;
}

void FrontPanel::showTransientNumber(int16_t value, uint32_t now,
                                     uint16_t durationMs) {
  transientNumber_ = value;
  transientEndsAt_ = now + durationMs;
  transientActive_ = true;
}

bool FrontPanel::acceptDisplayPayload(const uint8_t *payload, uint8_t length,
                                      uint32_t now) {
  if (payload == nullptr) return false;
  if (length == 2) {
    release();
    showTransientNumber(static_cast<int16_t>(readU16(payload)), now, 3000);
    return true;
  }
  if (length < 4 || payload[0] > kDisplayScheduledSegments ||
      payload[3] > 40) return false;
  const uint8_t target = payload[0];
  const uint8_t textLength = payload[3];
  const bool scheduled = target == kDisplayScheduledSegments;
  const uint8_t textOffset = scheduled ? 8 : 4;
  if (target == kDisplayReleaseHostPanel && textLength == 0) {
    release();
    return true;
  }
  const bool capture = target == 3;
  if (target != kDisplaySegments && !scheduled && !capture) return false;
  if (capture && textLength != 4) return false; // No fitted LCD: segment capture only.
  if ((!scheduled && length < static_cast<uint8_t>(4 + textLength)) ||
      (scheduled &&
       (length < 8 || length < static_cast<uint8_t>(8 + textLength) ||
        (payload[4] & kRepeatMask) > 2 ||
        (payload[4] & 0x5CU) != 0 ||
        ((payload[4] & kRepeatMask) == 2 && payload[7] == 0)))) {
    return false;
  }
  for (uint8_t index = 0; index < textLength; ++index) {
    const uint8_t value = payload[textOffset + index];
    if ((scheduled && (payload[4] & kRawCells) != 0) ||
        (value >= 0x20U && value <= 0x7EU)) continue;
    return false;
  }

  memset(hostText_, scheduled && (payload[4] & kRawCells) != 0 ? 0 : ' ',
         sizeof(hostText_));
  if (textLength != 0) memcpy(hostText_, payload + textOffset, textLength);
  hostText_[textLength] = '\0';
  hostTextLength_ = textLength;
  scrollIndex_ = 0;
  // Legacy senders have no repeat selector. Repetition now requires an
  // explicit scheduled request; long text alone must not capture the panel.
  options_ = scheduled ? payload[4] : 0;
  captured_ = capture;
  capturedMeta_ = capture ? readU16(payload + 1) : 0;
  stepMs_ = readU16(payload + 1);
  if (stepMs_ == 0) stepMs_ = 260;
  if (stepMs_ < 80) stepMs_ = 80;
  holdMs_ = scheduled ? readU16(payload + 5) : readU16(payload + 1);
  intervalSeconds_ = scheduled ? payload[7] : 0;
  const bool scrolling = textLength > 4 || (options_ & kForceScroll) != 0;
  nextAt_ = capture ? 0 : scrolling ? now + stepMs_
                     : (holdMs_ == 0 || (options_ & kRepeatMask) == 1
                            ? 0 : now + holdMs_);
  hostTextActive_ = textLength != 0;
  transientActive_ = false;
  animationActive_ = false;
  return true;
}

// This is the PCController marquee lifecycle: it walks beyond the final
// character to one blank frame before stopping, looping, or interval-waiting.
bool FrontPanel::showHostText(uint32_t now) {
  const uint8_t repeat = options_ & kRepeatMask;
  const bool scrolling = hostTextLength_ > 4 || (options_ & kForceScroll) != 0;
  if ((options_ & kIntervalWaiting) != 0) {
    if (!timeReached(now, nextAt_)) return false;
    options_ &= static_cast<uint8_t>(~kIntervalWaiting);
    scrollIndex_ = 0;
    nextAt_ = now + (scrolling ? stepMs_ : holdMs_);
  }
  if (!scrolling) {
    if (nextAt_ != 0 && timeReached(now, nextAt_)) {
      if (repeat == 2) {
        options_ |= kIntervalWaiting;
        nextAt_ = now + static_cast<uint32_t>(intervalSeconds_) * 1000UL;
      } else if (repeat == 0) {
        hostTextActive_ = false;
      }
      return false;
    }
    if ((options_ & kRawCells) != 0)
      display.showSegments(reinterpret_cast<const uint8_t *>(hostText_));
    else
      display.showText(hostText_);
    return true;
  }
  if (timeReached(now, nextAt_)) {
    if (scrollIndex_ < hostTextLength_) {
      ++scrollIndex_;
      nextAt_ = now + stepMs_;
    } else if (repeat == 1) {
      scrollIndex_ = 0;
      nextAt_ = now + stepMs_;
    } else if (repeat == 2) {
      options_ |= kIntervalWaiting;
      nextAt_ = now + static_cast<uint32_t>(intervalSeconds_) * 1000UL;
      return false;
    } else {
      hostTextActive_ = false;
      return false;
    }
  }
  char window[5];
  const bool raw = (options_ & kRawCells) != 0;
  for (uint8_t index = 0; index < 4; ++index) {
    const uint8_t source = static_cast<uint8_t>(scrollIndex_ + index);
    window[index] = source < hostTextLength_ ? hostText_[source] : (raw ? 0 : ' ');
  }
  window[4] = '\0';
  if (raw) display.showSegments(reinterpret_cast<const uint8_t *>(window));
  else display.showText(window);
  return true;
}

void FrontPanel::showLocal(BoardCore::Protocol::GameMode mode,
                           uint8_t selectedCell, uint8_t winner) {
  if (mode == BoardCore::Protocol::kModeFirstWins) {
    if (winner != 0) display.showInteger(winner);
    else display.showText(F("rEAd"));
  } else if (mode == BoardCore::Protocol::kModeCellSelect) {
    (void)selectedCell;
    display.showText(F("PAKr"));
  } else if (mode == BoardCore::Protocol::kModeSnake) {
    display.showText(F("SnAk"));
  } else {
    display.showText(F("IdLE"));
  }
}

void FrontPanel::service(uint32_t now, BoardCore::Protocol::GameMode mode,
                         uint8_t selectedCell, uint8_t winner,
                         bool programming) {
  if (static_cast<uint32_t>(now - lastRefreshAt_) < 20U) return;
  lastRefreshAt_ = now;
  if (programming) {
    display.showText(F("Prog"));
    return;
  }
  if (espActive_) { showEspMenu(now); return; }
  if (animationActive_) {
    // Imported from ControllerBoardMini/Puzzles::animation(): the lit segment
    // traces the exact twelve-step perimeter at the original 75 ms cadence.
    const uint32_t elapsed = now - animationStartedAt_;
    const uint8_t step = static_cast<uint8_t>(elapsed / 75U);
    if (step < 12) {
      uint8_t segments[4] = {};
      switch (step) {
        case 0: segments[0] = kSegmentA; break;
        case 1: segments[1] = kSegmentA; break;
        case 2: segments[2] = kSegmentA; break;
        case 3: segments[3] = kSegmentA; break;
        case 4: segments[3] = kSegmentB; break;
        case 5: segments[3] = kSegmentC; break;
        case 6: segments[3] = kSegmentD; break;
        case 7: segments[2] = kSegmentD; break;
        case 8: segments[1] = kSegmentD; break;
        case 9: segments[0] = kSegmentD; break;
        case 10: segments[0] = kSegmentE; break;
        case 11: segments[0] = kSegmentF; break;
      }
      display.showSegments(segments);
      return;
    }
    animationActive_ = false;
  }
  if (hostTextActive_ && showHostText(now)) return;
  if (transientActive_) {
    if (!timeReached(now, transientEndsAt_)) {
      display.showInteger(transientNumber_);
      return;
    }
    transientActive_ = false;
  }
  if (menuActive_) {
    if (menuPage_ == 3) display.showText(menuValue_ ? F("MutE") : F("bEEP"));
    else if (menuPage_ == 5) display.showText(menuValue_ ? F("run ") : F("IdLE"));
    else display.showInteger(menuValue_);
    return;
  }
  showLocal(mode, selectedCell, winner);
}
