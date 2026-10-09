#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TM1637Display.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <SerialLink.h>

#include "BoardPins.h"
#include "ShiftRegisters.h"
#include "TonePlayer.h"

using namespace BoardCore::Protocol;

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kDebounceMs = 25;
constexpr uint16_t kShiftPeriodMs = 5;
constexpr uint16_t kHeartbeatMs = 1000;

SoftwareSerial espSerial(BoardPins::EspSerialRx, BoardPins::EspSerialTx);
BoardCore::SerialLink link(static_cast<Stream &>(espSerial));
TM1637Display display(BoardPins::Tm1637Clock, BoardPins::Tm1637Data);

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
uint32_t lastHeartbeatAt = 0;
uint8_t eventSequence = 0;

void showState() {
  if (mode == kModeFirstWins && winner != 0) {
    display.clear();
    display.showNumberDec(winner, false, 1, 3);
  } else if (mode == kModeCellSelect) {
    display.showNumberDec(selectedCell, true, 4, 0);
  } else {
    const uint8_t dashes[] = {SEG_G, SEG_G, SEG_G, SEG_G};
    display.setSegments(dashes);
  }
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
  };
  link.send(kGameState, sequence, payload, sizeof(payload));
}

void sendCellEvent(Effect effect) {
  const uint8_t payload[] = {selectedCell, static_cast<uint8_t>(selectedState),
                             static_cast<uint8_t>(effect)};
  link.send(kCellEvent, eventSequence++, payload, sizeof(payload));
}

void resetGame() {
  winner = 0;
  shiftRegisters.allOutputsOff();
  if (mode == kModeCellSelect) {
    selectedCell = 1;
    selectedState = kCellWhite;
  }
  showState();
  tones.welcome();
}

void chooseWinner(uint8_t button) {
  if (mode != kModeFirstWins || winner != 0 || button < 1 || button > 9) return;
  winner = button;
  shiftRegisters.setActiveOutputs(button <= 8 ? _BV(button - 1) : 0);
  showState();
  tones.winner(button);
  sendGameState(eventSequence++);
}

void handleCellKey(uint8_t button) {
  switch (button) {
    case 1: selectedCell = selectedCell <= 1 ? 80 : selectedCell - 1; break;
    case 2: selectedCell = selectedCell >= 80 ? 1 : selectedCell + 1; break;
    case 3: selectedState = kCellRed; sendCellEvent(kEffectFade); break;
    case 4: selectedState = kCellOff; sendCellEvent(kEffectFade); break;
    case 5: selectedState = kCellWhite; sendCellEvent(kEffectFade); break;
    case 6: selectedCell = selectedCell <= 10 ? 1 : selectedCell - 10; break;
    case 7: selectedCell = selectedCell >= 71 ? 80 : selectedCell + 10; break;
    case 8: sendCellEvent(kEffectPulse); break;
    case 9: resetGame(); sendCellEvent(kEffectImmediate); break;
  }
  showState();
  sendGameState(eventSequence++);
}

void onInputChanged(uint8_t button, bool pressed) {
  const uint8_t payload[] = {button, static_cast<uint8_t>(pressed),
                             static_cast<uint8_t>(mode)};
  link.send(kInputEvent, eventSequence++, payload, sizeof(payload));
  if (!pressed) return;
  if (mode == kModeFirstWins) chooseWinner(button);
  else if (mode == kModeCellSelect) handleCellKey(button);
}

void handleFrame(const BoardCore::Protocol::Frame &frame, void *) {
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
      sendGameState(eventSequence++);
      break;
    case kCellAction:
      if (frame.payloadLength < 2 || frame.payload[0] < 1 ||
          frame.payload[0] > 80 || frame.payload[1] > kCellOff) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      selectedCell = frame.payload[0];
      selectedState = static_cast<CellState>(frame.payload[1]);
      showState();
      link.ack(frame.sequence, frame.opcode);
      sendGameState(eventSequence++);
      break;
    case kGameReset:
      resetGame();
      link.ack(frame.sequence, frame.opcode);
      sendGameState(eventSequence++);
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
      tones.clear();
      tones.add(static_cast<uint16_t>(frame.payload[0] | frame.payload[1] << 8),
                static_cast<uint16_t>(frame.payload[2] | frame.payload[3] << 8));
      link.ack(frame.sequence, frame.opcode);
      break;
    case kDisplayText:
      if (frame.payloadLength != 2) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      display.showNumberDec(static_cast<int16_t>(frame.payload[0] |
                                                 frame.payload[1] << 8),
                            false, 4, 0);
      link.ack(frame.sequence, frame.opcode);
      break;
    case kPing:
      link.send(kPong, frame.sequence, frame.payload, frame.payloadLength);
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
  const bool raw9 = analogRead(BoardPins::NinthButtonAdc) <
                    BoardPins::NinthButtonThreshold;

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
  tones.begin(BoardPins::Buzzer);
  display.setBrightness(5, true);
  showState();
  Serial.begin(115200);
  espSerial.begin(kBaud);
  link.begin(kBaud, handleFrame);
  delay(20);
  sendHello();
  tones.welcome();
}

void loop() {
  const uint32_t now = millis();
  link.service();
  serviceInputs(now);
  tones.service(now);
  if (static_cast<uint32_t>(now - lastHeartbeatAt) >= kHeartbeatMs) {
    lastHeartbeatAt = now;
    sendGameState(eventSequence++);
  }
}
