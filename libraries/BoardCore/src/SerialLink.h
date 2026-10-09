#pragma once

#include <Arduino.h>

#include "Protocol.h"

namespace BoardCore {

using FrameHandler = void (*)(const Protocol::Frame &frame, void *context);

class SerialLink {
 public:
  explicit SerialLink(HardwareSerial &serial) : serial_(serial), hardware_(&serial) {}
  explicit SerialLink(Stream &serial) : serial_(serial) {}

  void begin(uint32_t baud, FrameHandler handler, void *context = nullptr) {
    handler_ = handler;
    context_ = context;
    receiveLength_ = 0;
    dropping_ = false;
    if (hardware_ != nullptr) hardware_->begin(baud);
  }

  void service() {
    uint16_t pending = serial_.available();
    while (pending-- != 0) {
      const uint8_t value = static_cast<uint8_t>(serial_.read());
      if (value == 0) {
        if (!dropping_ && receiveLength_ != 0) {
          Protocol::Frame frame = {};
          if (Protocol::decode(receive_, receiveLength_, frame)) {
            if (handler_ != nullptr) handler_(frame, context_);
          } else if (errors_ != UINT16_MAX) {
            ++errors_;
          }
        }
        receiveLength_ = 0;
        dropping_ = false;
      } else if (dropping_) {
        continue;
      } else if (receiveLength_ < sizeof(receive_)) {
        receive_[receiveLength_++] = value;
      } else {
        dropping_ = true;
        receiveLength_ = 0;
        if (errors_ != UINT16_MAX) ++errors_;
      }
    }
  }

  bool send(uint8_t opcode, uint8_t sequence, const uint8_t *payload = nullptr,
            uint8_t payloadLength = 0) {
    if (payloadLength > Protocol::kMaximumPayload ||
        (payloadLength != 0 && payload == nullptr)) return false;
    Protocol::Frame frame = {};
    frame.opcode = opcode;
    frame.sequence = sequence;
    frame.payloadLength = payloadLength;
    if (payloadLength != 0) memcpy(frame.payload, payload, payloadLength);
    uint8_t encoded[Protocol::kMaximumEncoded];
    const uint8_t length = Protocol::encode(frame, encoded, sizeof(encoded));
    return length != 0 && serial_.write(encoded, length) == length;
  }

  bool ack(uint8_t sequence, uint8_t requestOpcode) {
    const uint8_t payload[] = {requestOpcode, Protocol::kNoError};
    return send(Protocol::kAck, sequence, payload, sizeof(payload));
  }

  bool error(uint8_t sequence, uint8_t requestOpcode, Protocol::Error error) {
    const uint8_t payload[] = {requestOpcode, static_cast<uint8_t>(error)};
    return send(Protocol::kErrorResponse, sequence, payload, sizeof(payload));
  }

  uint16_t errors() const { return errors_; }

 private:
  Stream &serial_;
  HardwareSerial *hardware_ = nullptr;
  FrameHandler handler_ = nullptr;
  void *context_ = nullptr;
  uint8_t receive_[Protocol::kMaximumEncoded] = {};
  uint8_t receiveLength_ = 0;
  bool dropping_ = false;
  uint16_t errors_ = 0;
};

}  // namespace BoardCore
