#pragma once

#include <Arduino.h>

#include "BoardPins.h"
#include "pitches.h"

// TonePlayer queues nonblocking tones on the board's Timer1 buzzer output.
class TonePlayer {
public:
  TonePlayer() : TonePlayer(BoardPins::Buzzer) {}
  // ATmega328P implementation owns Timer1 and the PB1/OC1A pin. It uses
  // hardware compare toggling (no audio-rate ISR); do not combine it with
  // Servo or analogWrite() on D9/D10.
  explicit TonePlayer(uint8_t pin);

  void begin(uint8_t pin);
  void begin();
  bool enqueue(uint16_t frequencyHz, uint16_t durationMs);
  bool pause(uint16_t durationMs);
  void beep(uint16_t durationMs = 40, uint16_t frequencyHz = 2000);
  void success();
  void error();
  void update(uint32_t now = millis());
  void stop();
  void setMuted(bool muted);
  bool isBusy() const;
  uint8_t revision() const { return revision_; }
  uint16_t activeFrequencyHz() const { return activeFrequencyHz_; }
  uint16_t activeDurationMs() const { return activeDurationMs_; }
  uint32_t activeStartedMicros() const { return activeStartedMs_ * 1000UL; }
  bool muted() const { return muted_; }
  // Stream commands: begin=0, append=1, start=2, finish=3, query=4, cancel=5.
  // Header [action,idLE16,offsetLE16]; append carries packed [HzLE16,msLE16].
  // action6: atomic flash cue [6,idLE16,0,0,onLE16,offLE16,count,lastOnLE16].
  // action7: atomic finite cue [7,idLE16,0,0,{HzLE16,msLE16} x 1..10].
  // Autonomous playback reports ordinary Playing (2), zero append credits;
  // stop/cue/reset cancels it. The internal state needs no public new enum.
  // Returns 0 accepted, 3 invalid, 5 full/busy, 6 muted or stale stream.
  uint8_t stream(const uint8_t *data, uint8_t length);
  void streamStatus(uint8_t *out) const;

private:
  // ToneStep is one queued tone or silent pause and its duration.
  struct ToneStep {
    uint16_t frequencyHz;
    uint16_t durationMs;
  };

  static constexpr uint8_t MAX_TONES = 10;

  bool startHardwareTone(uint16_t frequencyHz);
  void stopHardwareTone();

  uint8_t pin_;
  ToneStep queue_[MAX_TONES];
  uint8_t head_;
  uint8_t tail_;
  uint8_t count_;
  uint32_t stepEndsAt_;
  bool stepActive_;
  bool muted_;
  uint8_t revision_;
  uint16_t activeFrequencyHz_;
  uint16_t activeDurationMs_;
  uint32_t activeStartedMs_ = 0;
  uint16_t streamId_=0, streamOffset_=0;
  uint8_t streamState_=0; // idle, prefill, playing, complete, underrun, cancelled
  bool streamFinal_=false;
};

// buzzer is the single board-wide feedback player.
extern TonePlayer buzzer;
