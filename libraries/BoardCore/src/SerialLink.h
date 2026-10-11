#pragma once

#include <Arduino.h>

#include "Protocol.h"
#include "UartFlow.h"

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
    uint16_t pending = UartFlow::receiveBudget(serial_.available());
    while (pending-- != 0) {
      const int read = serial_.read();
      if (read < 0) break; // Occupancy changed; do not invent an 0xFF byte.
      const uint8_t value = static_cast<uint8_t>(read);
#if defined(ESP8266) || defined(ESP32)
      ++bytesReceived_;
#endif
      if (value == 0) {
        if (!dropping_ && receiveLength_ != 0) {
          Protocol::Frame frame = {};
          if (Protocol::decode(receive_, receiveLength_, frame)) {
#if defined(ESP8266) || defined(ESP32)
            ++framesReceived_;
#endif
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
    if (length == 0) return false;
    // A partial transport write has an unknown outcome. Never replay it.
    // End its incomplete COBS packet before a subsequent independent frame.
    if (resyncTransmit_) {
      const uint8_t delimiter = 0;
      if (serial_.write(&delimiter, 1) != 1) return false;
      resyncTransmit_ = false;
#if defined(ESP8266) || defined(ESP32)
      ++bytesTransmitted_;
#endif
    }
    const size_t written = serial_.write(encoded, length);
    if (written != length) {
      resyncTransmit_ = written != 0;
      return false;
    }
#if defined(ESP8266) || defined(ESP32)
    bytesTransmitted_ += length;
    ++framesTransmitted_;
#endif
    return true;
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
#if defined(ESP8266) || defined(ESP32)
  uint32_t bytesReceived() const { return bytesReceived_; }
  uint32_t bytesTransmitted() const { return bytesTransmitted_; }
  uint32_t framesReceived() const { return framesReceived_; }
  uint32_t framesTransmitted() const { return framesTransmitted_; }
#endif

 private:
  Stream &serial_;
  HardwareSerial *hardware_ = nullptr;
  FrameHandler handler_ = nullptr;
  void *context_ = nullptr;
  uint8_t receive_[Protocol::kMaximumEncoded] = {};
  uint8_t receiveLength_ = 0;
  bool dropping_ = false;
  bool resyncTransmit_ = false;
  uint16_t errors_ = 0;
#if defined(ESP8266) || defined(ESP32)
  uint32_t bytesReceived_ = 0;
  uint32_t bytesTransmitted_ = 0;
  uint32_t framesReceived_ = 0;
  uint32_t framesTransmitted_ = 0;
#endif
};

}  // namespace BoardCore
