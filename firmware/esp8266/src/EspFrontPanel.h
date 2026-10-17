#pragma once
#include <Protocol.h>

// Actual ESP settings are owned by the existing network/configuration service.
// This adapter never changes a shadow copy then ACKs it as saved. Its callbacks
// MUST validate, persist, apply and read back the same owner used by REST/Web.
class EspFrontPanel {
 public:
  using Snapshot = BoardCore::Protocol::EspPanel::Snapshot;
  class Settings {
   public:
    virtual bool read(Snapshot &state) const = 0;
    virtual BoardCore::Protocol::Error setMode(uint8_t mode) = 0;
    virtual BoardCore::Protocol::Error setDhcp(bool enabled) = 0;
    // Accepted actions are queued nonblocking; Scan is NOT completed SSID data.
    virtual BoardCore::Protocol::Error reconnect() = 0;
    virtual BoardCore::Protocol::Error scan() = 0;
  };
  template<class Sender>
  bool offer(Settings &settings, Sender &sender, uint8_t sequence) {
    using namespace BoardCore::Protocol;
    Snapshot state;
    if (!settings.read(state)) return false;
    uint8_t p[39] = {EspPanel::Offer};
    const uint8_t n = EspPanel::encodeSnapshot(state, p + 1);
    Snapshot verified;
    if (!EspPanel::decodeSnapshot(p + 1, n, verified)) return false;
    return sender.send(kEspPanel, sequence, p, n + 1);
  }
  template<class Sender>
  bool handle(const BoardCore::Protocol::Frame &frame, uint32_t now,
              Settings &settings, Sender &sender) {
    using namespace BoardCore::Protocol;
    if (frame.opcode != kEspPanel) return false;
    if (!frame.payloadLength || frame.payload[0] == EspPanel::Offer) return false;
    const uint8_t action = frame.payload[0];
    // Retransmitted exact request reuses the actual outcome, not a second save
    // or reconnect. Sequence reuse with different bytes is never a replay hit.
    if (cached_ && uint32_t(now - at_) < 2000 && frame.sequence == sequence_ &&
        frame.payloadLength == requestLength_ &&
        !memcmp(frame.payload, request_, requestLength_)) {
      sender.send(kEspPanelResponse, sequence_, response_, responseLength_);
      return true;
    }
    Error error = kBadPayload;
    const bool parameter = action == EspPanel::SetMode || action == EspPanel::SetDhcp;
    Snapshot before;
    if (frame.payloadLength != (parameter ? 2 : 1)) error = kBadPayload;
    else if (!settings.read(before)) error = kHardwareUnavailable;
    else switch (action) {
      case EspPanel::Query: error = kNoError; break;
      case EspPanel::SetMode:
        if (frame.payload[1] < 1 || frame.payload[1] > 3 ||
            ((frame.payload[1] & 2) && !before.stationConfigured)) error = kUnsafe;
        else error = settings.setMode(frame.payload[1]);
        break;
      case EspPanel::SetDhcp:
        error = frame.payload[1] > 1 ? kBadPayload : settings.setDhcp(frame.payload[1]);
        break;
      case EspPanel::Reconnect:
        error = before.stationConfigured ? settings.reconnect() : kUnsafe; break;
      case EspPanel::Scan: error = settings.scan(); break;
      default: error = kUnsupported; break;
    }
    Snapshot after;
    response_[0] = action;
    responseLength_ = 2;
    if (!settings.read(after)) error = kHardwareUnavailable;
    else {
      if (error == kNoError &&
          ((action == EspPanel::SetMode && after.mode != frame.payload[1]) ||
           (action == EspPanel::SetDhcp && after.dhcp != (frame.payload[1] != 0))))
        error = kHardwareUnavailable;
      const uint8_t n = EspPanel::encodeSnapshot(after, response_ + 2);
      Snapshot verified;
      if (!EspPanel::decodeSnapshot(response_ + 2, n, verified)) error = kHardwareUnavailable;
      else responseLength_ += n;
    }
    response_[1] = error;
    // Cache only bounded requests. Oversized input is replied to but never
    // copied into the two-byte cache.
    cached_ = frame.payloadLength <= sizeof(request_);
    requestLength_ = frame.payloadLength;
    if (cached_) memcpy(request_, frame.payload, requestLength_);
    sequence_ = frame.sequence; at_ = now;
    sender.send(kEspPanelResponse, sequence_, response_, responseLength_);
    return true;
  }
 private:
  uint8_t request_[2] = {}, response_[40] = {};
  uint8_t requestLength_ = 0, responseLength_ = 0, sequence_ = 0;
  uint32_t at_ = 0;
  bool cached_ = false;
};
