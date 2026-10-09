#include "TonePlayer.h"

TonePlayer tones;

void TonePlayer::begin(uint8_t pin) {
  pin_ = pin;
  pinMode(pin_, OUTPUT);
  clear();
}

void TonePlayer::clear() {
  noTone(pin_);
  length_ = 0;
  index_ = 0;
  finishesAt_ = 0;
}

bool TonePlayer::add(uint16_t frequency, uint16_t durationMs) {
  if (length_ >= kCapacity) return false;
  queue_[length_++] = {frequency, durationMs};
  return true;
}

bool TonePlayer::busy() const {
  return index_ < length_ || static_cast<int32_t>(millis() - finishesAt_) < 0;
}

void TonePlayer::service(uint32_t now) {
  if (static_cast<int32_t>(now - finishesAt_) < 0 || index_ >= length_) return;
  const Note &note = queue_[index_++];
  finishesAt_ = now + note.durationMs;
  if (note.frequency == 0) noTone(pin_);
  else tone(pin_, note.frequency, note.durationMs);
  if (index_ >= length_) {
    length_ = 0;
    index_ = 0;
  }
}

void TonePlayer::welcome() {
  clear();
  add(1032, 70); add(0, 60); add(2010, 70); add(0, 60);
  add(2400, 120); add(0, 150);
}

void TonePlayer::winner(uint8_t winnerNumber) {
  clear();
  const uint16_t base = static_cast<uint16_t>(440 + winnerNumber * 35);
  add(base, 90); add(0, 50); add(base + 220, 110); add(0, 50);
  add(base + 440, 260);
}

void TonePlayer::error() {
  clear();
  add(900, 100); add(0, 80); add(500, 220);
}
