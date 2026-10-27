#include "TonePlayer.h"

#include <avr/interrupt.h>

static_assert(BoardPins::Buzzer == PIN_PB1,
              "Timer1 OC1A buzzer output must remain on PB1 / Arduino D9");
static_assert(F_CPU == 16000000UL,
              "TonePlayer's compact Timer1 divider assumes the 16 MHz profile");

TonePlayer buzzer;

TonePlayer::TonePlayer(uint8_t pin)
    : pin_(pin), head_(0), tail_(0), count_(0), stepEndsAt_(0),
      stepActive_(false), muted_(false), revision_(0),
      activeFrequencyHz_(0), activeDurationMs_(0) {}

void TonePlayer::begin(uint8_t pin) {
  pin_ = pin;
  begin();
}

void TonePlayer::begin() {
  pinMode(pin_, OUTPUT);
  stop();
}

bool TonePlayer::enqueue(uint16_t frequencyHz, uint16_t durationMs) {
  if (durationMs == 0 || count_ >= MAX_TONES ||
      (frequencyHz != 0 && (frequencyHz < 20 || frequencyHz > 20000))) {
    return false;
  }

  queue_[tail_] = {frequencyHz, durationMs};
  tail_ = static_cast<uint8_t>((tail_ + 1) % MAX_TONES);
  ++count_;
  return true;
}

bool TonePlayer::pause(uint16_t durationMs) {
  return enqueue(0, durationMs);
}

void TonePlayer::beep(uint16_t durationMs, uint16_t frequencyHz) {
  enqueue(frequencyHz, durationMs);
}

void TonePlayer::success() {
  stop();
  enqueue(NOTE_C6, 70);
  pause(30);
  enqueue(NOTE_E6, 110);
}

void TonePlayer::error() {
  stop();
  enqueue(NOTE_E4, 90);
  pause(50);
  enqueue(NOTE_C4, 160);
}

void TonePlayer::update(uint32_t now) {
  if(streamState_==1)return; // Prefill is silent until an explicit start.
  uint32_t startsAt = now;
  if (stepActive_) {
    if (static_cast<int32_t>(now - stepEndsAt_) < 0) {
      return;
    }
    stopHardwareTone();
    stepActive_ = false;
    startsAt = stepEndsAt_;
  }
  // Keep the donor's musical deadlines, not loop-delay accumulation. If a
  // service call is late, omit fully expired steps and play only the remaining
  // portion of the current one. Work stays bounded by the ten-entry queue.
  while (count_ != 0) {
    const ToneStep step = queue_[head_];
    head_ = static_cast<uint8_t>((head_ + 1) % MAX_TONES);
    --count_;
    const uint32_t endsAt = startsAt + step.durationMs;
    if (static_cast<int32_t>(now - endsAt) >= 0) { startsAt = endsAt; continue; }
    activeFrequencyHz_ = step.frequencyHz;
    activeDurationMs_ = step.durationMs;
    activeStartedMs_ = startsAt;
    ++revision_;
    if (!muted_ && step.frequencyHz != 0) startHardwareTone(step.frequencyHz);
    else stopHardwareTone();
    stepEndsAt_ = endsAt;
    stepActive_ = true;
    return;
  }
  if (activeFrequencyHz_ != 0 || activeDurationMs_ != 0) {
    activeFrequencyHz_ = 0; activeDurationMs_ = 0; ++revision_;
    activeStartedMs_ = now;
  }
  if(streamState_==2)streamState_=streamFinal_?3:4;
}

void TonePlayer::stop() {
  if(streamState_==1||streamState_==2)streamState_=5;
  stopHardwareTone();
  head_ = 0;
  tail_ = 0;
  count_ = 0;
  stepEndsAt_ = 0;
  stepActive_ = false;
  if (activeFrequencyHz_ != 0 || activeDurationMs_ != 0) {
    activeFrequencyHz_ = 0;
    activeDurationMs_ = 0;
    activeStartedMs_ = millis();
    ++revision_;
  }
}

void TonePlayer::setMuted(bool muted) {
  if (muted_ == muted) return;
  muted_ = muted;
  ++revision_; // Publish mute changes even when no new note is scheduled.
  if (muted_) {
    stopHardwareTone();
  }
  // As in PCController, unmute never restarts an interrupted note. The
  // nonblocking timeline continues for host mirroring; only future steps sound.
}

bool TonePlayer::isBusy() const { return stepActive_ || count_ != 0; }

void TonePlayer::streamStatus(uint8_t *out) const {
  out[0]=streamId_;out[1]=streamId_>>8;
  out[2]=streamOffset_;out[3]=streamOffset_>>8;
  out[4]=MAX_TONES-count_;out[5]=streamState_;out[6]=muted_;
}

uint8_t TonePlayer::stream(const uint8_t *p,uint8_t n) {
  if(n<5)return 3;
  const uint8_t action=p[0];
  const uint16_t id=p[1]|uint16_t(p[2])<<8,offset=p[3]|uint16_t(p[4])<<8;
  if(action>5||!id||(action!=1&&n!=5))return 3;
  if(action==0){
    if(offset||muted_||isBusy()||id==streamId_)return 6;
    stop();streamId_=id;streamOffset_=0;streamFinal_=false;streamState_=1;return 0;
  }
  if(id!=streamId_)return 6;
  if(action==4)return 0;
  if(action==5){stop();streamState_=5;return 0;}
  if(muted_||(streamState_!=1&&streamState_!=2)||offset!=streamOffset_)return 6;
  if(action==2){if(streamState_!=1||!count_)return 6;streamState_=2;return 0;}
  if(action==3){streamFinal_=true;return 0;}
  if(streamFinal_||n==5||(n-5)%4)return 3;
  const uint8_t steps=(n-5)/4;
  if(steps>MAX_TONES-count_)return 5;
  if(uint32_t(streamOffset_)+steps>65535U)return 3;
  // Validate the complete chunk before changing the circular buffer.
  for(uint8_t at=5;at<n;at+=4){
    const uint16_t hz=p[at]|uint16_t(p[at+1])<<8,ms=p[at+2]|uint16_t(p[at+3])<<8;
    if(!ms||ms>5000||(hz&&(hz<20||hz>20000)))return 3;
  }
  for(uint8_t at=5;at<n;at+=4)
    enqueue(p[at]|uint16_t(p[at+1])<<8,p[at+2]|uint16_t(p[at+3])<<8);
  streamOffset_+=steps;return 0;
}

bool TonePlayer::startHardwareTone(uint16_t frequencyHz) {
  if (pin_ != BoardPins::Buzzer || frequencyHz == 0) {
    stopHardwareTone();
    return false;
  }

  // The native/host contract is 20..20,000 Hz. Every legal value fits Timer1
  // CTC with the /8 prescaler at 16 MHz, so a multi-prescaler search only
  // duplicated 32-bit division paths on the constrained image.
  if (frequencyHz < 20U || frequencyHz > 20000U) {
    stopHardwareTone();
    return false;
  }
  const uint16_t top = static_cast<uint16_t>(
      (1000000UL + frequencyHz / 2U) / frequencyHz - 1UL);

  const uint8_t savedStatus = SREG;
  cli();

  // Timer1 CTC with OCR1A as TOP and OC1A toggled by hardware. No Timer1 ISR
  // is enabled, so audio output cannot delay the INT0/INT1 RC edge handlers.
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 &= static_cast<uint8_t>(~_BV(OCIE1A));
  TCNT1 = 0;
  OCR1A = top;
  TIFR1 = _BV(OCF1A);
  TCCR1A = _BV(COM1A0);
  TCCR1B = static_cast<uint8_t>(_BV(WGM12) | _BV(CS11));

  SREG = savedStatus;
  return true;
}

void TonePlayer::stopHardwareTone() {
  const uint8_t savedStatus = SREG;
  cli();

  // Disconnect OC1A first, then stop Timer1's clock. Timer0 (millis/micros)
  // and Timer2 are deliberately untouched.
  TCCR1A &= static_cast<uint8_t>(~_BV(COM1A0));
  TCCR1B &= static_cast<uint8_t>(
      ~(_BV(CS12) | _BV(CS11) | _BV(CS10)));

  SREG = savedStatus;
  digitalWrite(pin_, LOW);
}
