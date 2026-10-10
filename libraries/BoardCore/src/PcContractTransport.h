#pragma once
#include <stdint.h>
#include <string.h>
#include "Protocol.h"
#include "pccontroller/ProtocolContract.h"
namespace BoardCore {
// Fixed-budget transaction ledger. Queued != device accepted. No mutation is
// replayed after timeout; late replies are quarantined for thirty seconds.
class PcContractTransport {
 public:
  enum State : uint8_t { Missing, Queued, Awaiting, Responded, OutcomeUnknown };
  struct Receipt {
    uint32_t id = 0;
    uint32_t changedAt = 0;
    State state = Missing;
    uint8_t wireSequence = 0, hostSequence = 0;
    bool usb = false;
    uint8_t requestLength = 0, responseLength = 0;
    uint8_t request[49] = {}, response[48] = {};
  };
  uint32_t enqueue(uint8_t opcode, const uint8_t *payload, uint8_t length,
                   uint32_t now, bool usb = false, uint8_t hostSequence = 0) {
    if (length > 48 || (length && !payload) || lastId_ == UINT32_MAX) return 0;
    Receipt *slot = nullptr;
    for (auto &entry : slots_) {
      if (entry.state == Missing) { slot = &entry; break; }
      if ((entry.state == Responded || entry.state == OutcomeUnknown) &&
          static_cast<uint32_t>(now - entry.changedAt) >= 60000) slot = &entry;
    }
    if (!slot) return 0; // Never overwrite another client's pending/result record.
    uint8_t seq = 0;
    for (uint16_t attempt = 0; attempt < 253; ++attempt) {
      if (++nextSequence_ > 253 || !nextSequence_) nextSequence_ = 1;
      if (!used_[nextSequence_] ||
          static_cast<uint32_t>(now - sequenceAt_[nextSequence_]) >= 30000) {
        bool pending = false;
        for (const auto &entry : slots_) pending |=
            entry.wireSequence == nextSequence_ &&
            (entry.state == Queued || entry.state == Awaiting);
        if (!pending) { seq = nextSequence_; break; }
      }
    }
    if (!seq) return 0;
    used_[seq] = true; sequenceAt_[seq] = now;
    *slot = Receipt{};
    slot->id = ++lastId_; slot->changedAt = now; slot->state = Queued;
    slot->wireSequence = seq; slot->usb = usb; slot->hostSequence = hostSequence;
    slot->requestLength = length + 1; slot->request[0] = opcode;
    if (length) memcpy(slot->request + 1, payload, length);
    return slot->id;
  }
  Receipt *next() {
    Receipt *selected = nullptr;
    for (auto &entry : slots_)
      if (entry.state == Queued && (!selected || entry.id < selected->id)) selected = &entry;
    return selected;
  }
  void sent(Receipt &entry, bool success, uint32_t now) {
    entry.state = success ? Awaiting : OutcomeUnknown; entry.changedAt = now;
    sequenceAt_[entry.wireSequence] = now;
  }
  Receipt *accept(const Protocol::Frame &frame, uint32_t now) {
    if (frame.opcode != Protocol::kPcContractResponse || !frame.payloadLength) return nullptr;
    for (auto &entry : slots_) {
      if (entry.state != Awaiting || entry.wireSequence != frame.sequence) continue;
      const uint8_t response = frame.payload[0], request = entry.request[0];
      if (response == ControllerProtocol::Ack || response == ControllerProtocol::ErrorResponse) {
        if (frame.payloadLength != 3 || frame.payload[1] != request ||
            frame.payload[2] > Protocol::kUnsafe ||
            (response == ControllerProtocol::Ack && frame.payload[2] != Protocol::kNoError)) return nullptr;
      } else if (response != expectedResponse(request)) return nullptr;
      entry.responseLength = frame.payloadLength;
      memcpy(entry.response, frame.payload, frame.payloadLength);
      entry.state = Responded; entry.changedAt = now; sequenceAt_[entry.wireSequence] = now;
      return &entry;
    }
    return nullptr;
  }
  template<class Observer> void expire(uint32_t now, Observer observer) {
    for (auto &entry : slots_) {
      const uint32_t timeout = entry.state == Queued ? 2000U : 750U;
      if ((entry.state == Awaiting || entry.state == Queued) &&
          static_cast<uint32_t>(now - entry.changedAt) >= timeout) {
        entry.state = OutcomeUnknown; entry.changedAt = now;
        sequenceAt_[entry.wireSequence] = now; observer(entry);
      }
    }
  }
  const Receipt *get(uint32_t id) const {
    for (const auto &entry : slots_) if (entry.id == id && entry.state != Missing) return &entry;
    return nullptr;
  }
  // USB responses need no REST retention; keep sequence quarantine separately.
  void releaseUsb(Receipt &entry) { if (entry.usb) entry.state = Missing; }
  bool consume(uint32_t id, uint8_t opcode) {
    for (auto &entry : slots_) {
      if (entry.id != id || entry.usb || entry.request[0] != opcode) continue;
      if (entry.state != Responded && entry.state != OutcomeUnknown) return false;
      entry.state = Missing; // Keep sequence quarantine independently.
      return true;
    }
    return false;
  }
  static bool decodeHex(const char *text, uint8_t *output, uint8_t &length) {
    if (!text || !output) return false;
    const size_t size = strlen(text);
    if (size > 96 || (size & 1U)) return false;
    length = 0;
    for (size_t i = 0; i < size; i += 2) {
      const int8_t a = digit(text[i]), b = digit(text[i + 1]);
      if (a < 0 || b < 0) return false;
      output[length++] = static_cast<uint8_t>((a << 4) | b);
    }
    return true;
  }
 private:
  static int8_t digit(char c) {
    return c >= '0' && c <= '9' ? c - '0' :
           c >= 'a' && c <= 'f' ? c - 'a' + 10 :
           c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
  }
  static uint8_t expectedResponse(uint8_t request) {
    using namespace ControllerProtocol;
    switch (request) {
      case Hello: return HelloResponse; case GetStatus: return StatusResponse;
      case GetSettings: return SettingsResponse; case FrontPanelGet: return FrontPanelResponse;
      case MenuList: return MenuListResponse; case MenuLayoutGet: return MenuLayoutResponse;
      case MacroStep: return MacroStatusResponse; case AutomationList: return AutomationListResponse;
      case AutomationPut: return AutomationRecordResponse; default: return Ack;
    }
  }
  Receipt slots_[8];
  uint32_t sequenceAt_[254] = {};
  bool used_[254] = {};
  uint8_t nextSequence_ = 0;
  uint32_t lastId_ = 0;
};
}
