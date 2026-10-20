#include <Arduino.h>

#include <BuildInfo.h>
#include <Protocol.h>
#include <GameRole.h>
#include <PcContractAssembly.h>
#include <SerialLink.h>
#include <UartFlow.h>
#include <Tasks.h>

#include "BootMelody.h"
#include <PeerCompatibility.h>
#include "BoardPins.h"
#include "BusyInput.h"
#include "FrontPanel.h"
#include "PakarBoardConfig.h"
#include "PcControllerRuntime.h"
#include <avr/wdt.h>
#include "ProgrammingLatch.h"
#include "ResetTelemetry.h"
#include "SevenSegments.h"
#include "ShiftRegisters.h"
#include "TonePlayer.h"

using namespace BoardCore::Protocol;

namespace {

constexpr uint32_t kBaud = 38400;
constexpr uint16_t kShiftPeriodMs = 5;

BoardCore::SerialLink link(Serial);

GameMode mode = static_cast<GameMode>(BoardCore::GameRole::mode);
CellState selectedState = kCellWhite;
uint8_t selectedCell = 1;
uint8_t winner = 0;
#if PAKAR_FIRST_WINS_ENABLED
uint16_t enabledKeys = 0; // Fail closed until the ESP has supplied its saved mask.
#endif
uint8_t stableInputs = 0;
bool stableInput9 = false;
uint32_t lastShiftAt = 0;
bool tm1637Ack = false;
uint8_t cellActionRevision = 0;
uint8_t resetRevision = 0;
Effect cellActionEffect = kEffectImmediate;
bool stateDirty = true;
Key boardKeys[9];
BoardCore::Tasks taskManager;
int8_t resetTask = -1;
void executeDeferredReset(void *);
void cancelDeferredReset() {
  taskManager.cancelTask(resetTask);
  resetTask = -1;
}

bool espReadyForUnsolicitedFrame();
void recordCellAction(Effect effect);
void showState();
void markStateDirty();

class BoardHardware final : public PcControllerHardware {
 public:
  Snapshot snapshot() const override {
    Snapshot s{};
    memcpy(s.segments, display.rawSegments(), 4);
    s.brightness = display.brightness(); s.inputs = stableInputs;
    s.input9 = stableInput9; s.outputs = shiftRegisters.activeOutputs();
    s.mode = mode; s.cell = selectedCell; s.winner = winner;
    s.displayDetected = display.detected(); s.hostDisplay = frontPanel.hostActive();
    s.hostCaptured = frontPanel.captured(); s.panelMeta = frontPanel.capturedMeta();
    s.programming = programmingLatch.active(); s.buzzerBusy = buzzer.isBusy();
    s.muted = buzzer.muted(); s.frequency = buzzer.activeFrequencyHz();
    s.duration = buzzer.activeDurationMs(); s.audioRevision = buzzer.revision();
    s.audioStartedMicros = buzzer.activeStartedMicros();
    s.uartErrors = link.errors(); return s;
  }
  Error execute(uint8_t opcode, const uint8_t *p, uint8_t n, uint32_t now) override {
    if (programmingLatch.active()) return kUnsafe;
    if (opcode == ControllerProtocol::Buzzer) {
      if (n != 4) return kBadPayload;
      const uint16_t hz = p[0] | static_cast<uint16_t>(p[1]) << 8;
      const uint16_t ms = p[2] | static_cast<uint16_t>(p[3]) << 8;
      if (!ms || (hz && (hz < 20 || hz > 20000))) return kBadPayload;
      buzzer.stop();
      return buzzer.enqueue(hz, ms) ? kNoError : kBusy;
    }
    if (opcode == ControllerProtocol::DisplayText) {
      if (n >= 4 && (p[0] == 1 || p[0] == 2)) return kHardwareUnavailable; // LCD absent.
      if (n < 4 || n != (p[0] == 5 ? 8 : 4) + p[3]) return kBadPayload;
      if (!frontPanel.acceptDisplayPayload(p, n, now)) return kBadPayload;
      markStateDirty(); return kNoError;
    }
    if (opcode == kOutputMask) {
      if (n != 1) return kBadPayload;
      shiftRegisters.setActiveOutputs(p[0]); markStateDirty(); return kNoError;
    }
    if (opcode == ControllerProtocol::Reset) {
      if (n != 1 || p[0] > 1) return kBadPayload;
      // Both targets reset through Urboot; target 1 is a hint, not an ISP write.
      cancelDeferredReset();
      resetTask = taskManager.addTask(100, executeDeferredReset, nullptr, now);
      return resetTask >= 0 ? kNoError : kBusy;
    }
    return kUnsupported;
  }
  void applySettings(const BoardSettings &s) override {
    buzzer.setMuted(programmingLatch.active() || (s.flags & 1U));
    markStateDirty();
  }
  Error menuAction(uint8_t action, uint8_t page) override {
    if (programmingLatch.active()) return kUnsafe;
    if (page == 0) {
      selectedCell = action == 2 ? (selectedCell <= 1 ? 80 : selectedCell - 1) :
                                  (selectedCell >= 80 ? 1 : selectedCell + 1);
      showState(); markStateDirty(); return kNoError;
    }
    if (page == 1) {
      const uint8_t mask = shiftRegisters.activeOutputs();
      shiftRegisters.setActiveOutputs(action == 2 ? static_cast<uint8_t>(mask - 1) :
                                                   static_cast<uint8_t>(mask + 1));
      markStateDirty(); return kNoError;
    }
    return kUnsafe; // Read-only key monitor.
  }
  void showMenu(uint8_t page, bool active, bool running) override {
    int16_t value = selectedCell;
    if (page == 1) value = shiftRegisters.activeOutputs();
    else if (page == 2) value = stableInputs | (stableInput9 ? 256 : 0);
    else if (page == 3) value = buzzer.muted();
    else if (page == 4) value = display.brightness();
    else if (page == 5) value = running;
    frontPanel.setMenu(page, active, value);
  }
  void showEspMenu(bool active, uint8_t page, uint8_t notice, uint8_t value,
                    const char *hostname, uint32_t now) override {
    frontPanel.setEspMenu(active, page, notice, value, hostname, now);
  }
  void menuNavigationCue(bool forward) override {
    if (!programmingLatch.active() && !buzzer.muted())
      playAudioCue(forward ? kCueIncrease : kCueDecrease);
  }
  void safeStop() override {
    shiftRegisters.allOutputsOff(); buzzer.stop(); markStateDirty();
  }
  bool ready() const override { return espReadyForUnsolicitedFrame(); }
  bool send(uint8_t opcode, uint8_t sequence, const uint8_t *p, uint8_t n) override {
    return link.send(opcode, sequence, p, n);
  }
} boardHardware;
PcControllerRuntime pcController(boardHardware);
BoardCore::PcContractAssembly pcAssembly;
extern "C" char __heap_start;
extern "C" void *__brkval;
void sendTelemetry(uint8_t sequence) {
  uint8_t p[46]={0,BoardCore::BuildInfo::kVersionMajor,
      BoardCore::BuildInfo::kVersionMinor,BoardCore::BuildInfo::kVersionPatch};
  const uint32_t now=millis();memcpy(p+4,&now,4);
  char stack;
  const uint16_t freeRam=&stack-(__brkval?static_cast<char*>(__brkval):&__heap_start);
  memcpy(p+8,&freeRam,2);
  const uint16_t adc=analogRead(A6),hz=buzzer.activeFrequencyHz(),ms=buzzer.activeDurationMs();
  memcpy(p+10,&adc,2);memcpy(p+12,&hz,2);memcpy(p+14,&ms,2);p[16]=buzzer.muted();
  memcpy(p+17,&pcController.settings().values(),21);p[38]=pcController.settings().persisted();
  const uint16_t features=BoardCore::Features::enabled,errors=link.errors();
  memcpy(p+39,&features,2);p[41]=programmingLatch.active();memcpy(p+42,&errors,2);
  const uint16_t lost=pcController.lostEvents();memcpy(p+44,&lost,2);
  link.send(kTelemetryResponse,sequence,p,sizeof(p));
  // Additive companion: preserve the existing 46-byte telemetry payload.
  uint8_t reset[6]={resetTelemetry.cause()};
  const uint32_t boots=resetTelemetry.count();memcpy(reset+1,&boots,4);
  reset[5]=resetTelemetry.persisted();
  link.send(kResetTelemetryResponse,sequence,reset,sizeof(reset));
}

void applyProgrammingSafety() {
  cancelDeferredReset(); // A previously ACKed reset must not interrupt ISP.
  shiftRegisters.allOutputsOff();
  buzzer.stop();
  buzzer.setMuted(true);
  display.showText(F("Prog"));
}

void executeDeferredReset(void *) {
  resetTask = -1;
  if (programmingLatch.active()) return;
  boardHardware.safeStop(); wdt_enable(WDTO_15MS); while (true) {}
}

void showState() {
  if (mode == kModeFirstWins && winner != 0) {
    frontPanel.showTransientNumber(winner, millis(), 3000);
  } else if (mode == kModeCellSelect) {
    frontPanel.showTransientNumber(selectedCell, millis());
  }
  tm1637Ack = display.detected();
}

void sendHello(uint8_t sequence = 0) {
  const uint16_t capabilities = kCapShiftInputs | kCapShiftOutputs | kCapAdc9 |
                                kCapTm1637 | kCapDisplayText | kCapBuzzer
#if PAKAR_PC_MENU
                                | kCapEspPanel
#endif
                                ;
  const uint8_t payload[] = {
      0, kRoleAvr, // Reserved zero; explicit capability bits own availability.
      static_cast<uint8_t>(capabilities),
      static_cast<uint8_t>(capabilities >> 8), 80, 9,
      BoardCore::BuildInfo::kVersionMajor,
      BoardCore::BuildInfo::kVersionMinor,
      BoardCore::BuildInfo::kVersionPatch,
      BoardSettings::Schema,
      static_cast<uint8_t>(BoardCore::Features::enabled),
      static_cast<uint8_t>(BoardCore::Features::enabled >> 8),
      kCueCount, // Append-only explicit stable audio IDs [0, count).
      1, // Explicit timestamped tone telemetry acknowledgement, not an API generation.
      1, // Explicit finite maintenance-panel priority acknowledgement.
      BoardCore::GameRole::mode, // Baked-in business role; never a runtime mode menu.
  };
  link.send(kHelloResponse, sequence, payload, sizeof(payload));
}

bool sendGameState(uint8_t sequence = 0) {
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
      pcController.menuPage(), static_cast<uint8_t>(pcController.menuActive()),
      static_cast<uint8_t>(pcController.running()), static_cast<uint8_t>(frontPanel.captured()),
      static_cast<uint8_t>(frontPanel.capturedMeta()), static_cast<uint8_t>(frontPanel.capturedMeta() >> 8),
  };
  return link.send(kGameState, sequence, payload, sizeof(payload));
}

bool espReadyForUnsolicitedFrame() {
#if PAKAR_USE_BUSY_WIRE
  return AvrBusyInput::ready();
#elif PAKAR_UART_BUFFERED
  // Admit a whole bounded frame without blocking autonomous peripherals.
  // A failed send leaves stateDirty armed; do not mistake a queue attempt for
  // successful transmission, let alone for peer acceptance.
  return BoardCore::UartFlow::ready(false, Serial.availableForWrite(), kMaximumEncoded);
#else
  return false;
#endif
}

void markStateDirty() { stateDirty = true; }

void recordCellAction(Effect effect) {
  cellActionEffect = effect;
  ++cellActionRevision;
}

void resetGame(bool playWelcome = true) {
  ++resetRevision;
  winner = 0;
  shiftRegisters.allOutputsOff();
  if (mode == kModeCellSelect) {
    selectedCell = 1;
    selectedState = kCellWhite;
  }
  showState();
  if (playWelcome && !programmingLatch.active()) playBootMelody();
  markStateDirty();
}

void chooseWinner(uint8_t button) {
#if PAKAR_FIRST_WINS_ENABLED
  if (mode != kModeFirstWins || winner != 0 || button < 1 || button > 9 ||
      !(enabledKeys & (1U << (button-1)))) return;
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
#else
  (void)button;
#endif
}

void handleCellKey(uint8_t button) {
#if PAKAR_TABLE_SELECTION
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
#else
  (void)button;
#endif
}

void onInputChanged(uint8_t button, bool pressed) {
  markStateDirty();
  if (!pressed) return;
  if (mode == kModeFirstWins) chooseWinner(button);
  else if (mode == kModeCellSelect) handleCellKey(button);
}

void handleFrame(const BoardCore::Protocol::Frame &frame, void *) {
  static bool peerAdmitted=false;
  if(frame.opcode==kHello) {
    peerAdmitted=false;
    if(frame.payloadLength) {
      if(frame.payloadLength!=4 || (frame.payload[0]!=kRoleEsp && frame.payload[0]!=kRoleHost)) {
        link.error(frame.sequence,kHello,kBadPayload);return;
      }
      peerAdmitted=BoardCore::PeerCompatibility::numericAppAllowed(frame.payload[1],frame.payload[2],frame.payload[3]);
      if(!peerAdmitted){link.error(frame.sequence,kHello,kUnsupported);return;}
    }
  }
  if(BoardCore::PeerCompatibility::enforced && !peerAdmitted &&
     frame.opcode!=kHello && frame.opcode!=kGetStatus && frame.opcode!=kGetTelemetry && frame.opcode!=kPing) {
    link.error(frame.sequence,frame.opcode,kUnsupported);return;
  }
  if (programmingLatch.active() && frame.opcode != kHello &&
      frame.opcode != kGetStatus && frame.opcode != kProgrammingState &&
      frame.opcode != kPing && frame.opcode != kGetTelemetry &&
      frame.opcode != kPcContract && frame.opcode != kPcContractChunk &&
      frame.opcode != kEspPanel && frame.opcode != kEspPanelResponse) {
    link.error(frame.sequence, frame.opcode, kUnsafe);
    return;
  }
  switch (frame.opcode) {
    case kEspPanel:
    case kEspPanelResponse:
      // These frames carry ESP-owned settings readback only. They cannot
      // clear Prog or mutate AVR settings; no fictitious generic HostMenu ACK.
      if (pcController.observeEspPanel(frame, millis())) markStateDirty();
      else if (frame.opcode == kEspPanel)
        link.error(frame.sequence, frame.opcode,
                   PAKAR_PC_MENU ? kBadPayload : kUnsupported);
      break;
    case kGetTelemetry:
      if(frame.payloadLength) link.error(frame.sequence,frame.opcode,kBadPayload);
      else sendTelemetry(frame.sequence);
      break;
    case kPcContractChunk:
      if (pcAssembly.accept(frame.sequence, frame.payload, frame.payloadLength, millis()))
        pcController.handle(frame.sequence, pcAssembly.data(), 49, millis(), micros());
      break;
    case kPcContract:
      pcController.handle(frame.sequence, frame.payload, frame.payloadLength, millis(), micros());
      break;
    case kHello:
      pcController.companionHeartbeat(millis());
      sendHello(frame.sequence);
      break;
    case kGetStatus:
      sendGameState(frame.sequence);
#if !PAKAR_USE_BUSY_WIRE && !PAKAR_UART_BUFFERED
      pcController.companionPoll(millis(),micros());
#else
      pcController.companionHeartbeat(millis());
#endif
      break;
    case kSetMode:
      if (frame.payloadLength != 1 || !BoardCore::GameRole::accepts(frame.payload[0])) {
        link.error(frame.sequence, frame.opcode, kBadPayload);
        break;
      }
      mode = static_cast<GameMode>(frame.payload[0]);
      resetGame();
      link.ack(frame.sequence, frame.opcode);
      break;
    case kCellAction:
      if (!PAKAR_TABLE_SELECTION || frame.payloadLength < 2 || frame.payloadLength > 3 ||
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
      tm1637Ack = display.detected();
      markStateDirty();
      link.ack(frame.sequence, frame.opcode);
      break;
    case kPing:
      pcController.companionHeartbeat(millis());
      link.send(kPong, frame.sequence, frame.payload, frame.payloadLength);
      break;
#if PAKAR_FIRST_WINS_ENABLED
    case kKeyMask:
      if(frame.payloadLength!=2 || frame.payload[1]>1 ||
          !(frame.payload[0]|frame.payload[1])) {link.error(frame.sequence,frame.opcode,kBadPayload);break;}
      enabledKeys=frame.payload[0]|uint16_t(frame.payload[1])<<8;
      resetGame(false);link.ack(frame.sequence,frame.opcode);break;
#endif
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
        buzzer.setMuted((pcController.settings().values().flags & 1U) != 0);
        resetGame(false);
      }
      link.ack(frame.sequence, frame.opcode);
      markStateDirty();
      break;
    default:
      link.error(frame.sequence, frame.opcode, kUnsupported);
      break;
  }
}

void boardKeyGesture(uint8_t bit, KeyEvent event, void *) {
  const bool primary = event == KeyEvent::Down;
  if (event == KeyEvent::Down || event == KeyEvent::Up) {
    const bool pressed = event == KeyEvent::Down;
    if (bit < 8) {
      if (pressed) stableInputs |= _BV(bit);
      else stableInputs &= static_cast<uint8_t>(~_BV(bit));
    } else stableInput9 = pressed;
    markStateDirty();
  }
  pcController.key(bit, event, 0, millis(), micros());
  if (!pcController.menuActive() && !pcController.espMenuActive() &&
      !frontPanel.captured() && primary)
    onInputChanged(bit + 1, true);
}

void serviceInputs(uint32_t now) {
  if (static_cast<uint32_t>(now - lastShiftAt) < kShiftPeriodMs) return;
  lastShiftAt = now;
  shiftRegisters.service();
  const uint8_t raw = shiftRegisters.activeInputs();
  const bool raw9 = analogRead(PakarBoardConfig::NinthButtonAdc) <
                    pcController.settings().values().ninthThreshold;
  for (uint8_t bit = 0; bit < 9; ++bit) {
    boardKeys[bit].updateSample(bit < 8 ? (raw & _BV(bit)) != 0 : raw9, now);
  }
}
}  // namespace

void setup() {
  programmingLatch.begin();
  // .init3 has disabled stale WDT; Prog must keep the ISP EEPROM backup exact.
  resetTelemetry.begin(!programmingLatch.active());
#if PAKAR_USE_BUSY_WIRE
  pinMode(BoardPins::EspBusy, INPUT_PULLUP);
#endif
  shiftRegisters.begin();
  buzzer.begin(BoardPins::Buzzer);
  display.begin(5);
  frontPanel.begin(millis());
  if (programmingLatch.active()) applyProgrammingSafety();
  else tm1637Ack = display.detected();
  pcController.begin(millis());
  for (uint8_t bit = 0; bit < 9; ++bit) {
    boardKeys[bit].begin(bit);
    boardKeys[bit].setEventCallback(boardKeyGesture);
  }
  link.begin(kBaud, handleFrame);
  const uint8_t flags=pcController.settings().values().flags;
  if (!programmingLatch.active() && (flags & 2U) && !(flags & 1U) &&
      resetTelemetry.allowWelcome()) playBootMelody();
}

void loop() {
  const uint32_t now = millis();
  link.service();
  if (!programmingLatch.active()) serviceInputs(now);
#if PAKAR_UART_BUFFERED && PAKAR_FIRST_WINS_ENABLED
  // Publish the latched key/winner before display, RF and menu work.
  // Failed bounded enqueue retains the dirty flag; never block input scanning.
  if (stateDirty && espReadyForUnsolicitedFrame()) stateDirty = !sendGameState();
#endif
  buzzer.update(now);
  frontPanel.service(now, mode, selectedCell, winner,
                     programmingLatch.active());
  display.service(now);
  display.serviceBrightness(programmingLatch.active() ? 5 :
                            pcController.settings().values().displayBrightness, now);
  if (tm1637Ack != display.detected()) { tm1637Ack = display.detected(); markStateDirty(); }
  pcController.service(now, micros());
  boardHardware.showMenu(pcController.menuPage(), pcController.menuActive(), pcController.running());
  static uint8_t previousPanel[6] = {};
  const uint16_t panelMeta = frontPanel.capturedMeta();
  const uint8_t panel[6] = {pcController.menuPage(), static_cast<uint8_t>(pcController.menuActive()),
      static_cast<uint8_t>(pcController.running()), static_cast<uint8_t>(frontPanel.captured()),
      static_cast<uint8_t>(panelMeta), static_cast<uint8_t>(panelMeta >> 8)};
  if (memcmp(previousPanel, panel, sizeof(panel))) {
    memcpy(previousPanel, panel, sizeof(panel)); markStateDirty();
  }
  static uint8_t previousSegments[4] = {};
  static uint8_t previousBrightness = 0xFF;
  if (memcmp(previousSegments, display.rawSegments(), 4) ||
      previousBrightness != display.brightness()) {
    memcpy(previousSegments, display.rawSegments(), 4); markStateDirty();
    previousBrightness = display.brightness();
  }
  taskManager.update(now);
#if PAKAR_USE_BUSY_WIRE || PAKAR_UART_BUFFERED
  // Busy-wire builds are event-driven. Reconnect convergence comes from the
  // ESP's HELLO -> GET_STATUS exchange, so periodic unsolicited frames only
  // waste UART bandwidth and increase the chance of colliding with FastLED.
  if (stateDirty && espReadyForUnsolicitedFrame()) {
    stateDirty = !sendGameState();
  }
#endif
}
