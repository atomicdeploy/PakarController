#include "PcControllerRuntime.h"
#include <string.h>

namespace PC = ControllerProtocol;
namespace P = BoardCore::Protocol;
namespace {
uint16_t readLE16(const uint8_t *p) { return p[0] | static_cast<uint16_t>(p[1]) << 8; }
void put16(uint8_t *p, uint16_t n) { p[0] = n; p[1] = n >> 8; }
void put32(uint8_t *p, uint32_t n) { for (uint8_t i = 0; i < 4; ++i) { p[i] = n; n >>= 8; } }
constexpr uint8_t labels[6][4] = {
    {'C','E','L','L'}, {'O','U','T',' '}, {'K','E','Y','S'},
    {'S','N','D',' '}, {'D','I','S','P'}, {'H','O','S','T'}};
bool readOnly(uint8_t op) {
  return op == PC::Hello || op == PC::GetStatus || op == PC::GetSettings ||
      op == PC::FrontPanelGet || op == PC::MenuList || op == PC::MenuLayoutGet ||
      op == PC::HostMenuStateGet || op == PC::AutomationList;
}
}

void PcControllerRuntime::begin(uint32_t now) {
  settings_.begin();
  page_ = settings_.values().defaultPage;
  hardware_.applySettings(settings_.values());
#if PAKAR_PC_MACROS
  ring_.initialize(6); // Donor ControllerEventType::Macro.
#endif
#if PAKAR_PC_AUTOMATIONS
  boardAutomations.begin(); // No implicit EEPROM provisioning on blank flash.
  automationEvent(AutomationEventKind::Boot, 0, now);
#else
  (void)now;
#endif
}

bool PcControllerRuntime::reply(uint8_t opcode, uint8_t sequence, const uint8_t *payload,
                                uint8_t length) {
  if (length > 47 || (length && !payload)) return false;
  uint8_t envelope[48];
  envelope[0] = opcode;
  if (length) memcpy(envelope + 1, payload, length);
  return hardware_.send(P::kPcContractResponse, sequence, envelope, length + 1);
}
void PcControllerRuntime::result(uint8_t sequence, uint8_t request, P::Error error) {
  if (sequence == 0xFE) {
    if (resultCount_ == 8) {
      if (lostEvents_ != UINT16_MAX) ++lostEvents_;
#if PAKAR_PC_MACROS
      ring_.cancel(false); macroDirty_ = true;
#endif
      hardware_.safeStop(); return;
    }
    auto &r = results_[(resultHead_ + resultCount_) & 7U];
    r.opcode = request; r.error = error; ++resultCount_; return;
  }
  const uint8_t p[] = {request, static_cast<uint8_t>(error)};
  reply(error == P::kNoError ? PC::Ack : PC::ErrorResponse, sequence, p, sizeof(p));
}
void PcControllerRuntime::handle(uint8_t sequence, const uint8_t *payload, uint8_t length,
                                 uint32_t now, uint32_t nowUs) {
  if (!payload || !length || length > P::kMaximumPayload + 1) {
    result(sequence, P::kPcContract, P::kBadPayload); return;
  }
  const bool returning = !connected_;
  connected_ = true;
  lastHostAt_ = now;
  const P::Error error = dispatch(payload[0], sequence, payload + 1, length - 1, now, nowUs);
  if (error != P::kNoError) result(sequence, payload[0], error);
  if (returning) automationEvent(AutomationEventKind::Host, 1, now);
}

void PcControllerRuntime::status(uint8_t sequence, uint32_t now) {
  const auto s = hardware_.snapshot();
  // Profile-specific schema; no manufactured voltage/current/temperature zeros.
  uint8_t p[25] = {BoardSettings::Schema, s.mode, s.cell, s.winner, s.inputs,
      s.input9, s.outputs, page_, static_cast<uint8_t>(running_),
      static_cast<uint8_t>(connected_), static_cast<uint8_t>(s.programming),
      static_cast<uint8_t>(s.buzzerBusy), static_cast<uint8_t>(s.muted),
      static_cast<uint8_t>(s.displayDetected), s.brightness};
  put32(p + 15, now);
  put16(p + 19, s.uartErrors);
  put16(p + 21, lostEvents_);
  put16(p + 23, BoardCore::Features::enabled);
  reply(PC::StatusResponse, sequence, p, sizeof(p));
}
void PcControllerRuntime::frontPanel(uint8_t sequence) {
  const auto s = hardware_.snapshot();
  // Actual donor 47-byte schema-2 offsets, no nonexistent LCD text.
  uint8_t p[47] = {2};
  memcpy(p + 1, s.segments, 4);
  p[5] = s.brightness;
  p[6] = (s.segments[0] | s.segments[1] | s.segments[2] | s.segments[3]) ? 2 : 0;
  p[8] = 0x80U | (s.displayDetected ? 0x40U : 0U);
  memset(p + 9, ' ', 32);
  p[41] = s.inputs; p[42] = page_; p[43] = s.mode;
  p[44] = (s.hostCaptured ? 0x80U : 0U) | ((s.panelMeta >> 12) & 0x0FU);
  p[45] = s.panelMeta; p[46] = (s.panelMeta >> 8) & 0x0FU;
  reply(PC::FrontPanelResponse, sequence, p, sizeof(p));
}

P::Error PcControllerRuntime::dispatch(uint8_t op, uint8_t sequence,
    const uint8_t *p, uint8_t n, uint32_t now, uint32_t nowUs, bool replay) {
  (void)nowUs; // Some complete feature profiles omit menus and macros.
  const auto s = hardware_.snapshot();
  if (s.programming && !readOnly(op)) return P::kUnsafe;
  if (replay && (op == PC::MacroStart || op == PC::MacroStep ||
      op == PC::MacroCancel || op == PC::SetSettings || op == PC::SetStreamPeriod ||
      op == PC::MenuLayoutSet || op == PC::AutomationPut ||
      op == PC::AutomationRemove || op == PC::AutomationClear || readOnly(op)))
    return P::kUnsafe;
  switch (op) {
    case PC::Hello: {
      if (n) return P::kBadPayload;
      // Explicit dialect/profile/feature negotiation. Never masquerade as the
      // donor's schema-3 sensor board or reuse its incompatible capability bits.
      uint8_t response[8] = {BoardSettings::Schema, 1, 1, 48};
      put16(response + 4, BoardCore::Features::enabled);
      put16(response + 6, BoardCore::Features::requested);
      reply(PC::HelloResponse, sequence, response, sizeof(response)); return P::kNoError;
    }
    case PC::GetStatus:
      if (n) return P::kBadPayload;
      status(sequence, now); drainOne(nowUs, true); return P::kNoError;
    case PC::GetSettings: {
      if (n) return P::kBadPayload;
      uint8_t response[sizeof(BoardSettings) + 1];
      memcpy(response, &settings_.values(), sizeof(BoardSettings));
      response[sizeof(BoardSettings)] = settings_.persisted();
      reply(PC::SettingsResponse, sequence, response, sizeof(response)); return P::kNoError;
    }
    case PC::SetSettings: {
      if (n != sizeof(BoardSettings)) return P::kBadPayload;
      BoardSettings value;
      memcpy(&value, p, sizeof(value));
      if (!validBoardSettings(value)) return P::kBadPayload;
      if (!settings_.replace(value, now)) return P::kHardwareUnavailable;
      hardware_.applySettings(value);
      if (!(value.visiblePages & (1U << page_))) page_ = firstVisiblePage(value.visiblePages, value.packedOrder);
      break;
    }
    case PC::SetStreamPeriod: {
      if (n != 2 || (readLE16(p) && readLE16(p) < 100)) return P::kBadPayload;
      BoardSettings value = settings_.values();
      value.streamPeriodMs = readLE16(p);
      if (!settings_.replace(value, now, false)) return P::kHardwareUnavailable;
      lastStreamAt_ = now; break;
    }
    case PC::FrontPanelGet:
      if (n) return P::kBadPayload;
      frontPanel(sequence); return P::kNoError;
    case PC::Buzzer:
    case PC::DisplayText: {
      const auto error = hardware_.execute(op, p, n, now);
      if (error != P::kNoError) return error;
      break;
    }
    case PC::ProgramState:
      if (!n || p[0] > 1) return P::kBadPayload;
      running_ = p[0] != 0; break; // Application running != durable ISP Prog.
    case PC::Reset:
      if (n != 1 || p[0] > 1) return P::kBadPayload;
      hardware_.safeStop();
      {
        const auto error = hardware_.execute(op, p, n, now);
        if (error != P::kNoError) return error;
      }
      break; // Board reset ACK precedes watchdog.
#if PAKAR_PC_MENU
    case PC::MenuList: {
      if (n != 1 || p[0] >= 6) return P::kBadPayload;
      uint8_t response[40] = {1, 6, 0xFF, 0};
      uint8_t index = 4;
      for (uint8_t cursor = p[0]; cursor < 6; ++cursor) {
        response[index++] = cursor; response[index++] = cursor;
        memcpy(response + index, labels[cursor], 4); index += 4; ++response[3];
      }
      reply(PC::MenuListResponse, sequence, response, index); return P::kNoError;
    }
    case PC::MenuLayoutGet: {
      if (n) return P::kBadPayload;
      uint8_t response[7] = {2, 6, settings_.values().visiblePages, 0};
      memcpy(response + 4, settings_.values().packedOrder, 3);
      reply(PC::MenuLayoutResponse, sequence, response, sizeof(response)); return P::kNoError;
    }
    case PC::MenuLayoutSet: {
      if (n != 7 || p[0] != 2 || p[1] != 6 || p[3] ||
          firstVisiblePage(p[2], p + 4) == 0xFF) return P::kBadPayload;
      BoardSettings value = settings_.values();
      value.visiblePages = p[2]; memcpy(value.packedOrder, p + 4, 3);
      if (!(value.visiblePages & (1U << value.defaultPage)))
        value.defaultPage = firstVisiblePage(value.visiblePages, value.packedOrder);
      if (!settings_.replace(value, now)) return P::kHardwareUnavailable;
      if (!(value.visiblePages & (1U << page_))) page_ = value.defaultPage;
      break;
    }
    case PC::MenuSetPage:
      if (n != 1 || p[0] >= 6 || !(settings_.values().visiblePages & (1U << p[0]))) return P::kBadPayload;
      page_ = p[0]; menuActive_ = true; menuOwnedByHost_ = true; saveMenuPage(now); break;
    case PC::MenuAction:
    case PC::RemoteKeyGesture: {
      if (n != (op == PC::MenuAction ? 1 : 2) || p[0] > 3 ||
          (op == PC::RemoteKeyGesture && p[1] > static_cast<uint8_t>(KeyEvent::Up)))
        return P::kBadPayload;
      const KeyEvent gesture = op == PC::MenuAction ? KeyEvent::Down : static_cast<KeyEvent>(p[1]);
      key(p[0], gesture, 2, now, nowUs);
      if (lastKeyError_ != P::kNoError) return lastKeyError_;
      // Remote Down is acted on synchronously by key(), not after double-click.
      break;
    }
    // These three IDs are registry-only in the donor dispatcher. They are
    // reserved for a future host-directory adapter, not an imaginary LCD menu.
    case PC::HostMenuDirectory: case PC::HostMenuContent: case PC::HostMenuStateGet:
      return P::kUnsupported;
#endif
#if PAKAR_PC_MACROS
    case PC::MacroStart: case PC::MacroCancel: case PC::MacroStep:
      return macro(op, sequence, p, n, nowUs);
#endif
#if PAKAR_PC_AUTOMATIONS
    case PC::AutomationList: {
      if (n != 1 || p[0] >= AutomationStore::Capacity) return P::kBadPayload;
      uint8_t response[42] = {AutomationStore::Schema, 0, 0, boardAutomations.count(), 0xFF, 0};
      put16(response + 1, boardAutomations.generation());
      uint8_t cursor = p[0], index = 6;
      AutomationRecord record;
      for (; cursor < AutomationStore::Capacity && response[5] < 3; ++cursor) {
        if (!boardAutomations.get(cursor, record)) continue;
        memcpy(response + index, &record, sizeof(record)); index += sizeof(record); ++response[5];
      }
      for (; cursor < AutomationStore::Capacity; ++cursor)
        if (boardAutomations.get(cursor, record)) { response[4] = cursor; break; }
      reply(PC::AutomationListResponse, sequence, response, index); return P::kNoError;
    }
    case PC::AutomationPut: {
      if (n != sizeof(AutomationRecord)) return P::kBadPayload;
      AutomationRecord record; memcpy(&record, p, sizeof(record));
      if (!AutomationStore::valid(record)) return P::kBadPayload;
      if (!automationSupported(record)) return P::kHardwareUnavailable;
      if (!boardAutomations.put(record)) return P::kHardwareUnavailable;
      uint8_t response[3 + sizeof(record)] = {AutomationStore::Schema};
      put16(response + 1, boardAutomations.generation());
      memcpy(response + 3, &record, sizeof(record));
      reply(PC::AutomationRecordResponse, sequence, response, sizeof(response)); return P::kNoError;
    }
    case PC::AutomationRemove:
      if (n != 1 || p[0] >= AutomationStore::Capacity) return P::kBadPayload;
      if (!boardAutomations.remove(p[0])) return P::kHardwareUnavailable;
      break;
    case PC::AutomationClear: {
      if (n) return P::kBadPayload;
      const uint16_t previous = boardAutomations.generation();
      boardAutomations.clear();
      if (boardAutomations.generation() == previous) return P::kHardwareUnavailable;
      break;
    }
#endif
    case PC::TemperatureList: case PC::PwmSet: case PC::PwmAllOff: case PC::PwmGet:
    case PC::StatusRgb: case PC::StatusEffect: case PC::StatusProfileGet: case PC::StatusProfileSet:
    case PC::AddressableLed: case PC::RadioTransmit: case PC::RadioLearnStart:
    case PC::RadioLearnCancel: case PC::RadioLearnClear: case PC::RadioLearnList:
    case PC::RadioLearnRemove: case PC::RadioLearnReplace:
    case PC::RelaySet: case PC::RelaySide: case PC::RelayAllOff: case PC::RelayTest:
    case PC::I2cTransfer:
      return P::kHardwareUnavailable;
    default: return P::kUnsupported;
  }
  hardware_.showMenu(page_, menuActive_, running_);
  result(sequence, op, P::kNoError);
  return P::kNoError;
}

bool PcControllerRuntime::macroStatus(uint8_t opcode, uint8_t sequence) {
#if PAKAR_PC_MACROS
  const auto &state = ring_.status();
  return reply(opcode, sequence, reinterpret_cast<const uint8_t *>(&state), sizeof(state));
#else
  (void)opcode; (void)sequence;
  return true;
#endif
}
P::Error PcControllerRuntime::macro(uint8_t op, uint8_t sequence,
    const uint8_t *p, uint8_t n, uint32_t nowUs) {
#if PAKAR_PC_MACROS
  if (op == PC::MacroStart) {
    if (n != 4 || (p[1] & ~ControllerCore::MacroRing::KeepOutputsOnCancel)) return P::kBadPayload;
    if (ring_.active()) return P::kBusy;
    ring_.begin(p[0], p[1], readLE16(p + 2)); macroDirty_ = true;
  } else if (op == PC::MacroCancel) {
    if (n > 1 || (n && p[0] > 1)) return P::kBadPayload;
    ring_.cancel(n ? p[0] != 0 : ring_.defaultKeepOutputsOnCancel()); macroDirty_ = true;
  } else {
    if (!n) return P::kBadPayload;
    switch (p[0]) {
      case 0:
        if (n < 6 || !ring_.append(readLE16(p + 1), readLE16(p + 3), p + 5, n - 5, nowUs)) return P::kBadPayload;
        break;
      case 1:
        if (n != 1 || !ring_.start(nowUs)) return P::kBadPayload;
        if (!ring_.status().report.totalSteps) ring_.completeStep(true);
        macroDirty_ = true; break;
      case 2:
        if (n != 1) return P::kBadPayload;
        macroStatus(PC::MacroStatusResponse, sequence); return P::kNoError;
      case 3:
        if (n != 2 || ring_.active()) return P::kBusy;
        ring_.beginRecording(p[1], hardware_.snapshot().outputs, nowUs); macroDirty_ = true; break;
      case 4:
        if (n != 1 || !ring_.stopRecording()) return P::kBadPayload;
        macroDirty_ = true; break;
      case 5: {
        if (n != 3 || !ring_.hasRecording() || readLE16(p + 1) > ring_.status().report.acceptedBytes ||
            readLE16(p + 1) % ControllerCore::MacroRing::SnapshotBytes) return P::kBadPayload;
        uint8_t response[44] = {6, 0x80, p[1], p[2]};
        const uint8_t count = ring_.readRecording(readLE16(p + 1), response + 4, 40);
        reply(PC::MacroStatusResponse, sequence, response, count + 4); return P::kNoError;
      }
      case 6:
        // Raw LED shift mask is a Pakar command, never a fake RelaySet adapter.
        if (n != 1 || !ring_.startRecorded(nowUs, P::kOutputMask)) return P::kBadPayload;
        macroDirty_ = true; break;
      case 7:
        if (n != 1 || !ring_.clearRecording()) return P::kBadPayload;
        macroDirty_ = true; break;
      default: return P::kBadPayload;
    }
  }
  if (ring_.takeSafeStopRequest()) hardware_.safeStop();
  result(sequence, op, P::kNoError); return P::kNoError;
#else
  (void)op; (void)sequence; (void)p; (void)n; (void)nowUs;
  return P::kUnsupported;
#endif
}

void PcControllerRuntime::queueEvent(const uint8_t *p, uint8_t n) {
  if (!n || n > sizeof(events_[0].bytes) || eventCount_ == 8) {
    if (lostEvents_ != UINT16_MAX) ++lostEvents_;
    return;
  }
  auto &event = events_[(eventHead_ + eventCount_) & 7U];
  event.length = n; memcpy(event.bytes, p, n); ++eventCount_;
}
void PcControllerRuntime::key(uint8_t bit, KeyEvent gesture, uint8_t source,
                              uint32_t now, uint32_t nowUs) {
  lastKeyError_ = P::kNoError;
  uint8_t event[9] = {1, bit, static_cast<uint8_t>(gesture), source, 0xFF};
  put32(event + 5, nowUs);
  if (connected_) queueEvent(event, sizeof(event));
  automationEvent(AutomationEventKind::Key,
                   static_cast<uint8_t>((bit << 4) | static_cast<uint8_t>(gesture)), now);
#if PAKAR_PC_MENU
  if (bit == 8 && gesture == KeyEvent::DoubleClick) {
    menuActive_ = !menuActive_; menuOwnedByHost_ = false;
  }
  if (source == 2) { menuActive_ = true; menuOwnedByHost_ = true; }
  if (!hardware_.snapshot().hostCaptured && menuActive_ && bit < 4 &&
      keyEventRunsPrimaryAction(gesture)) {
    if (bit < 2) {
      uint8_t rank = 0;
      for (; rank < 6; ++rank)
        if (((settings_.values().packedOrder[rank >> 1] >> ((rank & 1) * 4)) & 15) == page_) break;
      for (uint8_t attempt = 0; attempt < 6; ++attempt) {
        rank = (rank + (bit == 0 ? 5 : 1)) % 6;
        const uint8_t page = (settings_.values().packedOrder[rank >> 1] >> ((rank & 1) * 4)) & 15;
        if (settings_.values().visiblePages & (1U << page)) { page_ = page; break; }
      }
    } else {
      if (page_ == 3 || page_ == 4) {
        BoardSettings value = settings_.values();
        if (page_ == 3) value.flags = (value.flags & ~1U) | (bit == 2 ? 1U : 0U);
        else value.displayBrightness = (value.displayBrightness + (bit == 2 ? 7 : 1)) % 8;
        if (!settings_.replace(value, now, false)) lastKeyError_ = P::kHardwareUnavailable;
        else hardware_.applySettings(value);
      } else if (page_ == 5) running_ = bit == 3;
      else lastKeyError_ = hardware_.menuAction(bit, page_);
    }
  }
#endif
  saveMenuPage(now);
  hardware_.showMenu(page_, menuActive_, running_);
}
void PcControllerRuntime::saveMenuPage(uint32_t now) {
#if PAKAR_PC_MENU
  if (!hardware_.snapshot().programming && (settings_.values().flags & 4U) &&
      settings_.values().defaultPage != page_) {
    BoardSettings value = settings_.values(); value.defaultPage = page_;
    if (!settings_.replace(value, now, false)) lastKeyError_ = P::kHardwareUnavailable;
  }
#else
  (void)now;
#endif
}

bool PcControllerRuntime::automationSupported(const AutomationRecord &r) const {
  if (r.eventKind != static_cast<uint8_t>(AutomationEventKind::Boot) &&
      r.eventKind != static_cast<uint8_t>(AutomationEventKind::Host) &&
      (r.eventKind != static_cast<uint8_t>(AutomationEventKind::Key) || (r.eventValue >> 4) > 8)) return false;
  if (r.actionKind == static_cast<uint8_t>(AutomationActionKind::Buzzer) &&
      (r.value < 20 || r.value > 20000)) return false;
  return r.actionKind == static_cast<uint8_t>(AutomationActionKind::SafeStop) ||
      r.actionKind == static_cast<uint8_t>(AutomationActionKind::Buzzer) ||
      r.actionKind == static_cast<uint8_t>(AutomationActionKind::HostMacroRequest);
}
bool PcControllerRuntime::automationAction(const AutomationRecord &r, void *context) {
  auto &self = *static_cast<PcControllerRuntime *>(context);
  if (!self.automationSupported(r) || self.hardware_.snapshot().programming) return false;
  bool success = true;
  if (r.actionKind == static_cast<uint8_t>(AutomationActionKind::SafeStop)) self.hardware_.safeStop();
  else if (r.actionKind == static_cast<uint8_t>(AutomationActionKind::Buzzer)) {
    uint8_t p[4]; put16(p, r.value); put16(p + 2, r.extra);
    success = self.hardware_.execute(PC::Buzzer, p, sizeof(p), self.actionNow_) == P::kNoError;
  }
  const uint8_t event[] = {13,
      static_cast<uint8_t>(r.actionKind == static_cast<uint8_t>(AutomationActionKind::HostMacroRequest) ? 3 : success ? 1 : 2),
      r.id, r.actionKind, r.actionTarget};
  if (self.connected_) self.queueEvent(event, sizeof(event));
  return success;
}
void PcControllerRuntime::automationEvent(AutomationEventKind kind, uint8_t value, uint32_t now) {
#if PAKAR_PC_AUTOMATIONS
  actionNow_ = now;
  executor_.dispatch(kind, value, now);
#else
  (void)kind; (void)value; (void)now;
#endif
}

void PcControllerRuntime::service(uint32_t now, uint32_t nowUs) {
  const auto s = hardware_.snapshot();
  settings_.service(now, !s.programming);
  if (connected_ && static_cast<uint32_t>(now - lastHostAt_) >= 5000) {
    connected_ = false;
#if PAKAR_PC_MACROS
    const bool active = ring_.active();
    ring_.cancel(false);
    if (active || running_) hardware_.safeStop();
    ring_.takeSafeStopRequest();
#else
    if (running_) hardware_.safeStop();
#endif
    running_ = false;
    if (menuOwnedByHost_) menuActive_ = false;
    const uint8_t release[] = {4, 0, 0, 0};
    hardware_.execute(PC::DisplayText, release, sizeof(release), now);
    automationEvent(AutomationEventKind::Host, 0, now);
    hardware_.showMenu(page_, menuActive_, false);
    eventHead_ = eventCount_ = 0; // Stale events never replay as fresh after reconnect.
    resultHead_ = resultCount_ = 0;
  }
#if PAKAR_PC_MACROS
  if (s.programming && ring_.active()) { ring_.cancel(false); hardware_.safeStop(); macroDirty_ = true; }
  if (!s.programming) {
    ring_.recordRelay(s.outputs, nowUs);
    ControllerCore::MacroRing::Command command;
    uint8_t p[48];
    const auto due = ring_.dequeueDue(nowUs, command, p, sizeof(p));
    if (due == ControllerCore::MacroRing::Ready) {
      const P::Error error = command.opcode == P::kOutputMask
          ? hardware_.execute(command.opcode, p, command.payloadLength, now)
          : dispatch(command.opcode, 0xFE, p, command.payloadLength, now, nowUs, true);
      if (command.opcode == P::kOutputMask || error != P::kNoError) result(0xFE, command.opcode, error);
      if (!ring_.active() || ring_.completeStep(error == P::kNoError)) macroDirty_ = true;
    } else if (due == ControllerCore::MacroRing::Malformed) macroDirty_ = true;
  }
  if (ring_.takeSafeStopRequest()) hardware_.safeStop();
#endif
  if (!connected_ || !hardware_.ready()) return;
  uint8_t segments[5];
  memcpy(segments, s.segments, 4);
  segments[4] = s.brightness | 0x80U | (s.displayDetected ? 0x40U : 0U);
  if (memcmp(segments, lastSegments_, sizeof(segments))) {
    if (reply(PC::SegmentChanged, 0, segments, sizeof(segments)))
      memcpy(lastSegments_, segments, sizeof(segments));
    return; // One bounded unsolicited frame per loop; no display/audio burst.
  }
  if (lastAudioRevision_ != s.audioRevision || lastMuted_ != s.muted) {
    uint8_t audio[9]; put16(audio, s.frequency); put16(audio + 2, s.duration);
    audio[4] = s.muted; put32(audio + 5, nowUs);
    if (reply(PC::BuzzerChanged, 0, audio, sizeof(audio))) {
      lastAudioRevision_ = s.audioRevision; lastMuted_ = s.muted;
    }
    return;
  }
  if (resultCount_ || eventCount_ || macroDirty_) { drainOne(); return; }
  if (settings_.values().streamPeriodMs &&
      static_cast<uint32_t>(now - lastStreamAt_) >= settings_.values().streamPeriodMs) {
    status(0, now); lastStreamAt_ = now;
  }
}
void PcControllerRuntime::drainOne(uint32_t nowUs, bool mirror) {
  // Polling builds have no unsolicited TX grant. A solicited status request
  // grants one bounded changed-only mirror/event frame after its own response.
  if (mirror) {
    const auto s = hardware_.snapshot();
    uint8_t segments[5];
    memcpy(segments, s.segments, 4);
    segments[4] = s.brightness | 0x80U | (s.displayDetected ? 0x40U : 0U);
    if (memcmp(segments, lastSegments_, sizeof(segments))) {
      if (reply(PC::SegmentChanged, 0, segments, sizeof(segments)))
        memcpy(lastSegments_, segments, sizeof(segments));
      return;
    }
    if (lastAudioRevision_ != s.audioRevision || lastMuted_ != s.muted) {
      uint8_t audio[9]; put16(audio, s.frequency); put16(audio + 2, s.duration);
      audio[4] = s.muted; put32(audio + 5, nowUs);
      if (reply(PC::BuzzerChanged, 0, audio, sizeof(audio))) {
        lastAudioRevision_ = s.audioRevision; lastMuted_ = s.muted;
      }
      return;
    }
  }
  if (resultCount_) {
    const auto &r = results_[resultHead_];
    const uint8_t payload[] = {r.opcode, r.error};
    if (reply(r.error ? PC::ErrorResponse : PC::Ack, 0xFE, payload, sizeof(payload))) {
      resultHead_ = (resultHead_ + 1) & 7U; --resultCount_;
    }
    return;
  }
  if (eventCount_) {
    const auto &event = events_[eventHead_];
    if (reply(PC::Event, 0, event.bytes, event.length)) {
      eventHead_ = (eventHead_ + 1) & 7U; --eventCount_;
    }
    return;
  }
  if (macroDirty_) { if (macroStatus(PC::Event, 0)) macroDirty_ = false; return; }
}
