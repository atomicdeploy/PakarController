#pragma once
#include <BoardFeatures.h>
#include <Protocol.h>
#include <pccontroller/ProtocolContract.h>
#include <pccontroller/MacroRing.h>
#include <pccontroller/AutomationStore.h>
#include <pccontroller/Keys.h>
#include <pccontroller/ControllerEvents.h>
#include "BoardSettings.h"

// Hardware adapter, not a parallel peripheral engine. Every mutation passes
// through the same board safety latch and the real existing drivers.
class PcControllerHardware {
 public:
  struct Snapshot {
    uint8_t segments[4];
    uint8_t brightness, inputs, input9, outputs, mode, cell, winner;
    bool displayDetected, hostDisplay, hostCaptured, programming, buzzerBusy, muted;
    uint16_t panelMeta;
    uint16_t frequency, duration, uartErrors;
    uint8_t audioRevision;
    uint32_t audioStartedMicros;
  };
  virtual Snapshot snapshot() const = 0;
  virtual BoardCore::Protocol::Error execute(uint8_t opcode, const uint8_t *payload,
                                            uint8_t length, uint32_t now) = 0;
  virtual void applySettings(const BoardSettings &settings) = 0;
  virtual BoardCore::Protocol::Error menuAction(uint8_t action, uint8_t page) = 0;
  virtual void showMenu(uint8_t page, bool active, bool running) = 0;
  virtual void showEspMenu(bool, uint8_t, uint8_t, uint8_t,
                           const char *, uint32_t) {}
  virtual void menuNavigationCue(bool) {}
  virtual void safeStop() = 0;
  virtual bool ready() const = 0;
  virtual bool send(uint8_t opcode, uint8_t sequence, const uint8_t *payload,
                    uint8_t length) = 0;
};

class PcControllerRuntime : public ControllerEventSink {
 public:
  explicit PcControllerRuntime(PcControllerHardware &hardware) : hardware_(hardware) {}
  void begin(uint32_t now);
  // Input is the legacy tunnel payload: [unmodified donor opcode, data].
  void handle(uint8_t sequence, const uint8_t *payload, uint8_t length,
              uint32_t now, uint32_t nowUs);
  void service(uint32_t now, uint32_t nowUs);
  void key(uint8_t bit, KeyEvent gesture, uint8_t source, uint32_t now, uint32_t nowUs);
  const BoardSettingsStore &settings() const { return settings_; }
  bool running() const { return running_; }
  bool menuActive() const { return menuActive_; }
  uint8_t menuPage() const { return page_; }
  uint16_t lostEvents() const { return lostEvents_; }
  // Feed actual ESP offers/results only, never an invented host-menu ACK.
  bool observeEspPanel(const BoardCore::Protocol::Frame &frame, uint32_t now);
  bool espMenuActive() const { return espMenuActive_; }
  bool espMenuAvailable(uint32_t now) const {
    return espKnown_ && static_cast<uint32_t>(now - espSeenAt_) < 5000;
  }
 private:
  BoardCore::Protocol::Error dispatch(uint8_t opcode, uint8_t sequence,
      const uint8_t *payload, uint8_t length, uint32_t now, uint32_t nowUs,
      bool replay = false);
  bool reply(uint8_t opcode, uint8_t sequence, const uint8_t *payload, uint8_t length);
  void result(uint8_t sequence, uint8_t request, BoardCore::Protocol::Error error);
  void status(uint8_t sequence, uint32_t now);
  void frontPanel(uint8_t sequence);
  bool macroStatus(uint8_t opcode, uint8_t sequence);
  BoardCore::Protocol::Error macro(uint8_t opcode, uint8_t sequence,
      const uint8_t *payload, uint8_t length, uint32_t nowUs);
  bool automationSupported(const AutomationRecord &record) const;
  static bool automationAction(const AutomationRecord &record, void *context);
  void automationEvent(AutomationEventKind kind, uint8_t value, uint32_t now);
  void queueEvent(const uint8_t *payload, uint8_t length);
  void event(const uint8_t *payload, uint8_t length) override { queueEvent(payload, length); }
  ControllerEvents eventCodec_{*this};
  struct PendingEvent { uint8_t length; uint8_t bytes[10]; };
  struct PendingResult { uint8_t opcode; uint8_t error; };
  PcControllerHardware &hardware_;
  BoardSettingsStore settings_;
#if PAKAR_PC_MACROS
  ControllerCore::MacroRing ring_{};
#endif
#if PAKAR_PC_AUTOMATIONS
  AutomationExecutor executor_{boardAutomations, automationAction, this};
#endif
  PendingEvent events_[8] = {};
  PendingResult results_[8] = {};
  uint8_t resultHead_ = 0, resultCount_ = 0;
  uint8_t eventHead_ = 0, eventCount_ = 0;
  uint16_t lostEvents_ = 0;
  uint8_t page_ = 0;
  uint8_t lastSegments_[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t lastAudioRevision_ = 0xFF;
  bool lastMuted_ = false;
  uint32_t lastHostAt_ = 0, lastStreamAt_ = 0;
  uint32_t actionNow_ = 0;
  bool connected_ = false, running_ = false, menuActive_ = false, macroDirty_ = false;
  bool menuOwnedByHost_ = false;
  void drainOne(bool mirror = false);
  void saveMenuPage(uint32_t now);
  void showEspPanel(uint32_t now);
  bool espKey(uint8_t bit, KeyEvent gesture, uint8_t source, uint32_t now);
  void requestEspAction(uint32_t now);
  BoardCore::Protocol::EspPanel::Snapshot espState_{};
  uint32_t espSeenAt_ = 0, espSentAt_ = 0, espNoticeAt_ = 0;
  uint8_t espPage_ = 0, espDraft_ = 1, espSequence_ = 0;
  uint8_t espPendingAction_ = 0, espNotice_ = 0;
  bool espKnown_ = false, espMenuActive_ = false, espPending_ = false;
  BoardCore::Protocol::Error lastKeyError_ = BoardCore::Protocol::kNoError;
};
