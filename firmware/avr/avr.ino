#include <Arduino.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "BootMelody.h"
#include "BoardPins.h"
#include "PakarBoardConfig.h"
#include "ProgrammingLatch.h"
#include "SevenSegments.h"
#include "ShiftRegisters.h"
#include "TonePlayer.h"

using namespace BoardCore::Protocol;

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kDebounceMs = 25;
constexpr uint16_t kShiftPeriodMs = 5;

BoardCore::SerialLink link(Serial);

GameMode mode = kModeCellSelect;
CellState selectedState = kCellWhite;
uint8_t selectedCell = 1;
uint8_t winner = 0;
uint8_t stableInputs = 0;
uint8_t candidateInputs = 0;
bool stableInput9 = false;
bool candidateInput9 = false;
uint32_t candidateChangedAt[9] = {};
uint32_t lastShiftAt = 0;
bool tm1637Ack = false;
uint8_t cellActionRevision = 0;
uint8_t resetRevision = 0;
Effect cellActionEffect = kEffectImmediate;

void playAudioCue(AudioCue cue) {
  buzzer.stop();
  switch (cue) {
    case kCueWelcome:
      playBootMelody();
      break;
    case kCueUpdateStart:
      buzzer.enqueue(NOTE_G5, 90);
      buzzer.pause(45);
      buzzer.enqueue(NOTE_E5, 90);
      buzzer.pause(45);
      buzzer.enqueue(NOTE_C5, 180);
      break;
    case kCueUpdateSuccess:
      buzzer.enqueue(NOTE_C5, 80);
      buzzer.pause(35);
      buzzer.enqueue(NOTE_E5, 90);
      buzzer.pause(35);
      buzzer.enqueue(NOTE_G5, 170);
      break;
    case kCueError:
      buzzer.error();
      break;
    case kCueWinner:
      buzzer.enqueue(NOTE_C6, 90);
      buzzer.pause(35);
      buzzer.enqueue(NOTE_G6, 180);
      break;
  }
}

void applyProgrammingSafety() {
  shiftRegisters.allOutputsOff();
  buzzer.stop();
  buzzer.setMuted(true);
  display.showText(F("Prog"));
}

bool probeTm1637() {
  pinMode(BoardPins::Tm1637Clock, OUTPUT);
  pinMode(BoardPins::Tm1637Data, OUTPUT);
  digitalWrite(BoardPins::Tm1637Clock, HIGH);
  digitalWrite(BoardPins::Tm1637Data, HIGH);
  delayMicroseconds(3);
  digitalWrite(BoardPins::Tm1637Data, LOW);
  delayMicroseconds(3);

  uint8_t value = 0x8D;  // Display on, brightness 5.
  for (uint8_t bit = 0; bit < 8; ++bit) {
    digitalWrite(BoardPins::Tm1637Clock, LOW);
    digitalWrite(BoardPins::Tm1637Data, value & 0x01U ? HIGH : LOW);
    delayMicroseconds(3);
    digitalWrite(BoardPins::Tm1637Clock, HIGH);
    delayMicroseconds(3);
    value >>= 1;
  }
  digitalWrite(BoardPins::Tm1637Clock, LOW);
  pinMode(BoardPins::Tm1637Data, INPUT_PULLUP);
  delayMicroseconds(3);
  digitalWrite(BoardPins::Tm1637Clock, HIGH);
  delayMicroseconds(3);
  const bool acknowledged = digitalRead(BoardPins::Tm1637Data) == LOW;
  digitalWrite(BoardPins::Tm1637Clock, LOW);
  pinMode(BoardPins::Tm1637Data, OUTPUT);
  digitalWrite(BoardPins::Tm1637Data, LOW);
  delayMicroseconds(3);
  digitalWrite(BoardPins::Tm1637Clock, HIGH);
  delayMicroseconds(3);
  digitalWrite(BoardPins::Tm1637Data, HIGH);
  return acknowledged;
}

void showState() {
  if (mode == kModeFirstWins && winner != 0) {
    display.showInteger(winner);
  } else if (mode == kModeCellSelect) {
    display.showInteger(selectedCell);
  } else {
    display.showUnavailable();
  }
  tm1637Ack = probeTm1637();
}

void sendHello(uint8_t sequence = 0) {
  const uint16_t capabilities = kCapShiftInputs | kCapShiftOutputs | kCapAdc9 |
                                kCapTm1637 | kCapBuzzer;
  const uint8_t payload[] = {
      BoardCore::BuildInfo::kProtocolVersion, kRoleAvr,
      static_cast<uint8_t>(capabilities),
      static_cast<uint8_t>(capabilities >> 8), 80, 9,
      BoardCore::BuildInfo::kVersionMajor,
      BoardCore::BuildInfo::kVersionMinor,
      BoardCore::BuildInfo::kVersionPatch,
  };
  link.send(kHelloResponse, sequence, payload, sizeof(payload));
}

void sendGameState(uint8_t sequence = 0) {
  const uint16_t errors = link.errors();
  const uint8_t payload[] = {
      static_cast<uint8_t>(mode), winner, selectedCell,
      static_cast<uint8_t>(selectedState), stableInputs,
      static_cast<uint8_t>(stableInput9), shiftRegisters.activeOutputs(),
      static_cast<uint8_t>(errors), static_cast<uint8_t>(errors >> 8),
      static_cast<uint8_t>(tm1637Ack),
      cellActionRevision, static_cast<uint8_t>(cellActionEffect), resetRevision,
      static_cast<uint8_t>(programmingLatch.active()),
  };
  link.send(kGameState, sequence, payload, sizeof(payload));
}

void recordCellAction(Effect effect) {
  cellActionEffect = effect;
  ++cellActionRevision;
}

void resetGame() {
  ++resetRevision;
  winner = 0;
  shiftRegisters.allOutputsOff();
  if (mode == kModeCellSelect) {
    selectedCell = 1;
    selectedState = kCellWhite;
  }
  showState();
  if (!programmingLatch.active()) playBootMelody();
}

void chooseWinner(uint8_t button) {
  if (mode != kModeFirstWins || winner != 0 || button < 1 || button > 9) return;
  winner = button;
  shiftRegisters.setActiveOutputs(button <= 8 ? _BV(button - 1) : 0);
  showState();
  buzzer.stop();
  const uint16_t base = static_cast<uint16_t>(440 + button * 35);
  buzzer.enqueue(base, 90);
  buzzer.pause(50);
  buzzer.enqueue(base + 220, 110);
  buzzer.pause(50);
  buzzer.enqueue(base + 440, 260);
}

void handleCellKey(uint8_t button) {
  switch (button) {
    case 1: selectedCell = selectedCell <= 1 ? 80 : selectedCell - 1; break;
    case 2: selectedCell = selectedCell >= 80 ? 1 : selectedCell + 1; break;
    case 3: selectedState = kCellRed; recordCellAction(kEffectFade); break;
    case 4: selectedState = kCellOff; recordCellAction(kEffectFade); break;
    case 5: selectedState = kCellWhite; recordCellAction(kEffectFade); break;
    case 6: selectedCell = selectedCell <= 10 ? 1 : selectedCell - 10; break;
    case 7: selectedCell = selectedCell >= 71 ? 80 : selectedCell + 10; break;
    case 8: recordCellAction(kEffectPulse); break;
    case 9: resetGame(); break;
  }
  showState();
}

void onInputChanged(uint8_t button, bool pressed) {
  if (!pressed) return;
  if (mode == kModeFirstWins) chooseWinner(button);
  else if (mode == kModeCellSelect) handleCellKey(button);
}

void handleFrame(const BoardCore::Protocol::Frame &frame, void *) {
  if (programmingLatch.active() && frame.opcode != kHello &&
      frame.opcode != kGetStatus && frame.opcode != kProgrammingState &&
      frame.opcode != kPing) {
    link.error(frame.sequence, frame.opcode, kUnsafe);
    return;
  }
  switch (frame.opcode) {
    case kHello:
      sendHello(frame.sequence);
      break;
    case kGetStatus:
      sendGameState(frame.sequence);
      break;
    case kSetMode:
      if (frame.payloadLength != 1 || frame.payload[0] > kModeFirstWins) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      mode = static_cast<GameMode>(frame.payload[0]);
      resetGame();
      link.ack(frame.sequence, frame.opcode);
      break;
    case kCellAction:
      if (frame.payloadLength < 2 || frame.payloadLength > 3 ||
          frame.payload[0] < 1 || frame.payload[0] > 80 ||
          frame.payload[1] > kCellOff ||
          (frame.payloadLength == 3 && frame.payload[2] > kEffectPulse)) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      selectedCell = frame.payload[0];
      selectedState = static_cast<CellState>(frame.payload[1]);
      recordCellAction(frame.payloadLength == 3
                           ? static_cast<Effect>(frame.payload[2])
                           : kEffectFade);
      showState();
      link.ack(frame.sequence, frame.opcode);
      break;
    case kGameReset:
      resetGame();
      link.ack(frame.sequence, frame.opcode);
      break;
    case kOutputMask:
      if (frame.payloadLength != 1) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      shiftRegisters.setActiveOutputs(frame.payload[0]);
      link.ack(frame.sequence, frame.opcode);
      break;
    case kBuzzer:
      if (frame.payloadLength != 4) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      buzzer.stop();
      buzzer.enqueue(
          static_cast<uint16_t>(frame.payload[0] | frame.payload[1] << 8),
          static_cast<uint16_t>(frame.payload[2] | frame.payload[3] << 8));
      link.ack(frame.sequence, frame.opcode);
      break;
    case kAudioCue:
      if (frame.payloadLength != 1 || frame.payload[0] > kCueWinner ||
          programmingLatch.active()) {
        link.error(frame.sequence, frame.opcode,
                   programmingLatch.active() ? kUnsafe : kBadPayload);
        break;
      }
      playAudioCue(static_cast<AudioCue>(frame.payload[0]));
      link.ack(frame.sequence, frame.opcode);
      break;
    case kDisplayText:
      if (frame.payloadLength != 2) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      display.showInteger(static_cast<int16_t>(frame.payload[0] |
                                               frame.payload[1] << 8));
      tm1637Ack = probeTm1637();
      link.ack(frame.sequence, frame.opcode);
      break;
    case kPing:
      link.send(kPong, frame.sequence, frame.payload, frame.payloadLength);
      break;
    case kProgrammingState:
      if (frame.payloadLength != 1 || frame.payload[0] > 1) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      if (!programmingLatch.set(frame.payload[0] != 0)) {
        link.error(frame.sequence, frame.opcode, kHardwareUnavailable);
        break;
      }
      if (programmingLatch.active()) {
        applyProgrammingSafety();
      } else {
        buzzer.setMuted(false);
        resetGame();
      }
      link.ack(frame.sequence, frame.opcode);
      break;
    default:
      link.error(frame.sequence, frame.opcode, kUnsupported);
      break;
  }
}

void serviceInputs(uint32_t now) {
  if (static_cast<uint32_t>(now - lastShiftAt) < kShiftPeriodMs) return;
  lastShiftAt = now;
  shiftRegisters.service();
  const uint8_t raw = shiftRegisters.activeInputs();
  const bool raw9 = analogRead(PakarBoardConfig::NinthButtonAdc) <
                    PakarBoardConfig::NinthButtonThreshold;

  for (uint8_t bit = 0; bit < 8; ++bit) {
    const bool value = (raw & _BV(bit)) != 0;
    const bool candidate = (candidateInputs & _BV(bit)) != 0;
    if (value != candidate) {
      if (value) candidateInputs |= _BV(bit); else candidateInputs &= ~_BV(bit);
      candidateChangedAt[bit] = now;
    } else if (static_cast<uint32_t>(now - candidateChangedAt[bit]) >= kDebounceMs) {
      const bool stable = (stableInputs & _BV(bit)) != 0;
      if (stable != value) {
        if (value) stableInputs |= _BV(bit); else stableInputs &= ~_BV(bit);
        onInputChanged(bit + 1, value);
      }
    }
  }

  if (raw9 != candidateInput9) {
    candidateInput9 = raw9;
    candidateChangedAt[8] = now;
  } else if (static_cast<uint32_t>(now - candidateChangedAt[8]) >= kDebounceMs &&
             stableInput9 != raw9) {
    stableInput9 = raw9;
    onInputChanged(9, raw9);
  }
}

}  // namespace

void setup() {
  shiftRegisters.begin();
  buzzer.begin(BoardPins::Buzzer);
  display.begin(5);
  programmingLatch.begin();
  if (programmingLatch.active()) applyProgrammingSafety();
  else showState();
  link.begin(kBaud, handleFrame);
  if (!programmingLatch.active()) playBootMelody();
}

void loop() {
  const uint32_t now = millis();
  link.service();
  if (!programmingLatch.active()) serviceInputs(now);
  buzzer.update(now);
}
