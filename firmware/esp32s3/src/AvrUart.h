#pragma once
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This adapter requires ESP32-S3, not ESP8266/C3/classic ESP32"
#endif
#include <Arduino.h>
#include <HardwareSerial.h>
#include <SerialLink.h>
#include <atomic>
#include "BoardPins.h"

namespace PakarS3 {
// Own UART1; Serial/USB is the independent host path. Keep the same existing
// COBS + CRC-8 envelope rather than adding another wire protocol or effects engine.
class AvrUart {
 public:
  AvrUart() : uart_(1), link_(static_cast<Stream &>(uart_)) {}
  AvrUart(const AvrUart &) = delete;
  AvrUart &operator=(const AvrUart &) = delete;
  bool begin(BoardCore::FrameHandler handler, void *context = nullptr) {
    if (started_) return false;
    if (uart_.setRxBufferSize(kAvrRxBuffer) != kAvrRxBuffer ||
        uart_.setTxBufferSize(kAvrTxBuffer) != kAvrTxBuffer) return false;
    uart_.onReceiveError([this](hardwareSerial_error_t error) {
      if (error != UART_NO_ERROR) hardwareErrors_.fetch_add(1, std::memory_order_relaxed);
    });
    uart_.begin(kAvrBaud, SERIAL_8N1, PAKAR_S3_AVR_RX, PAKAR_S3_AVR_TX);
    if (!uart_) {
      end();
      return false;
    }
    // Stream constructor is intentional: SerialLink::begin must NOT call
    // HardwareSerial::begin again and replace explicit pins/buffer settings.
    link_.begin(kAvrBaud, handler, context);
    started_ = true;
    return true;
  }
  void service() { if (started_) link_.service(); }
  BoardCore::SerialLink &frames() { return link_; }
  uint32_t hardwareErrors() const { return hardwareErrors_.load(std::memory_order_relaxed); }
  bool started() const { return started_; }
  void end() {
    uart_.onReceiveError(nullptr);
    uart_.end();
    started_ = false;
  }
 private:
  HardwareSerial uart_;
  BoardCore::SerialLink link_;
  std::atomic<uint32_t> hardwareErrors_{0};
  bool started_ = false;
};
} // namespace PakarS3
