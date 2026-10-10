#include <Arduino.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "BootMelody.h"
#include "BoardPins.h"
#include "FrontPanel.h"
#include "PakarBoardConfig.h"
#include "ProgrammingLatch.h"
#include "SevenSegments.h"
#include "ShiftRegisters.h"
#include "TonePlayer.h"

using namespace BoardCore::Protocol;

#ifndef PAKAR_USE_BUSY_WIRE
#define PAKAR_USE_BUSY_WIRE 1
#endif

#if PAKAR_USE_BUSY_WIRE != 0 && PAKAR_USE_BUSY_WIRE != 1
#error "PAKAR_USE_BUSY_WIRE must be 0 or 1"
#endif

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
bool stateDirty = true;

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
    frontPanel.showTransientNumber(winner, millis(), 3000);
  } else if (mode == kModeCellSelect) {
    frontPanel.showTransientNumber(selectedCell, millis());
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
  const uint8_t *segments = display.rawSegments();
  const uint8_t payload[] = {
      static_cast<uint8_t>(mode), winner, selectedCell,
      static_cast<uint8_t>(selectedState), stableInputs,
      static_cast<uint8_t>(stableInput9), shiftRegisters.activeOutputs(),
      static_cast<uint8_t>(errors), static_cast<uint8_t>(errors >> 8),
      static_cast<uint8_t>(tm1637Ack),
      cellActionRevision, static_cast<uint8_t>(cellActionEffect), resetRevision,
      static_cast<uint8_t>(programmingLatch.active()),
      segments[0], segments[1], segments[2], segments[3],
      display.brightness(), static_cast<uint8_t>(frontPanel.hostActive()),
  };
  link.send(kGameState, sequence, payload, sizeof(payload));
}

bool espReadyForUnsolicitedFrame() {
#if PAKAR_USE_BUSY_WIRE
  return digitalRead(BoardPins::EspBusy) == LOW;
#else
  return false;
#endif
}

void markStateDirty() { stateDirty = true; }

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
  markStateDirty();
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
  markStateDirty();
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
  markStateDirty();
}

void onInputChanged(uint8_t button, bool pressed) {
  markStateDirty();
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
      if (frame.payloadLength != 1 || frame.payload[0] > kModeSnake) {
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
      markStateDirty();
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
      markStateDirty();
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
      if (frame.payloadLength != 1 || frame.payload[0] >= kCueCount ||
          programmingLatch.active()) {
        link.error(frame.sequence, frame.opcode,
                   programmingLatch.active() ? kUnsafe : kBadPayload);
        break;
      }
      playAudioCue(static_cast<AudioCue>(frame.payload[0]));
      link.ack(frame.sequence, frame.opcode);
      break;
    case kDisplayText:
      if (!frontPanel.acceptDisplayPayload(frame.payload, frame.payloadLength,
                                           millis())) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      tm1637Ack = probeTm1637();
      markStateDirty();
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
      markStateDirty();
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
#if PAKAR_USE_BUSY_WIRE
  pinMode(BoardPins::EspBusy, INPUT_PULLUP);
#endif
  shiftRegisters.begin();
  buzzer.begin(BoardPins::Buzzer);
  display.begin(5);
  frontPanel.begin(millis());
  programmingLatch.begin();
  if (programmingLatch.active()) applyProgrammingSafety();
  else tm1637Ack = probeTm1637();
  link.begin(kBaud, handleFrame);
  if (!programmingLatch.active()) playBootMelody();
}

void loop() {
  const uint32_t now = millis();
  link.service();
  if (!programmingLatch.active()) serviceInputs(now);
  buzzer.update(now);
  frontPanel.service(now, mode, selectedCell, winner,
                     programmingLatch.active());
#if PAKAR_USE_BUSY_WIRE
  // Busy-wire builds are event-driven. Reconnect convergence comes from the
  // ESP's HELLO -> GET_STATUS exchange, so periodic unsolicited frames only
  // waste UART bandwidth and increase the chance of colliding with FastLED.
  if (stateDirty && espReadyForUnsolicitedFrame()) {
    sendGameState();
    stateDirty = false;
  }
#endif
}
