#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace PakarS3 {
// This transport-neutral helper is the actual capture store used below, not a
// simulated UART. No heap, clock, callbacks, decoder or device actions belong here.
template <size_t Capacity = 128> class PassiveRxCapture {
 public:
  static_assert(Capacity > 0 && Capacity <= 128, "passive capture must stay bounded");
  static constexpr uint32_t MaximumDurationMs = 8000;
  static constexpr uint8_t MaximumChunk = 32;
  enum class StopReason : uint8_t { None, Requested, Deadline, UnsafePins, DriverFailure };
  struct Snapshot {
    uint32_t baud = 0, startedAt = 0, elapsedMs = 0;
    uint32_t received = 0, dropped = 0;
    uint16_t stored = 0;
    bool active = false;
    StopReason stopReason = StopReason::None;
  };
  static bool validBaud(uint32_t baud) { return baud == 38400 || baud == 115200; }
  bool begin(uint32_t baud, uint32_t now) {
    if (!validBaud(baud) || state_.active) return false;
    state_ = Snapshot();
    state_.baud = baud; state_.startedAt = now; state_.active = true;
    return true;
  }
  void record(uint8_t byte) {
    if (!state_.active) return;
    if (state_.received != UINT32_MAX) ++state_.received;
    if (state_.stored < Capacity) bytes_[state_.stored++] = byte;
    else if (state_.dropped != UINT32_MAX) ++state_.dropped;
  }
  void service(uint32_t now) {
    if (!state_.active) return;
    state_.elapsedMs = static_cast<uint32_t>(now - state_.startedAt);
    if (state_.elapsedMs >= MaximumDurationMs) stop(now, StopReason::Deadline);
  }
  void stop(uint32_t now, StopReason reason = StopReason::Requested) {
    if (!state_.active) return;
    state_.elapsedMs = static_cast<uint32_t>(now - state_.startedAt);
    state_.active = false; state_.stopReason = reason;
    // Keep bytes, counts and capture identity until a successful new begin.
  }
  Snapshot snapshot() const { return state_; }
  uint8_t readChunk(uint16_t offset, uint8_t length, uint8_t *output) const {
    if (offset > Capacity || length > MaximumChunk ||
        (length && !output) || offset >= state_.stored) return 0;
    const uint16_t remaining = state_.stored - offset;
    const uint8_t count = remaining < length ? static_cast<uint8_t>(remaining) : length;
    if (count) memcpy(output, bytes_ + offset, count);
    return count;
  }
 private:
  Snapshot state_;
  uint8_t bytes_[Capacity] = {};
};
} // namespace PakarS3

// Intentionally absent from the ordinary UART-enabled firmware. The root owner
// must include this header after defining the quiet commissioning build flag.
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3) && \
    defined(PAKAR_AVR_LINK_ENABLED) && !PAKAR_AVR_LINK_ENABLED
#include <Arduino.h>
#include <HardwareSerial.h>
#include <atomic>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp32-hal-uart.h>

namespace PakarS3 {
class PassiveAvrRx {
 public:
  using Capture = PassiveRxCapture<128>;
  using StopReason = Capture::StopReason;
  enum class Error : uint8_t { None, BadBaud, AlreadyCapturing, UartOwned, UnsafePins, DriverFailure };
  struct Snapshot : Capture::Snapshot {
    uint32_t hardwareErrors = 0;
    Error error = Error::None;
    bool pinsSafe = false;
  };
  // Referencing the existing Serial2 owner does not begin a driver or touch pins.
  PassiveAvrRx() = default;
  PassiveAvrRx(const PassiveAvrRx &) = delete;
  PassiveAvrRx &operator=(const PassiveAvrRx &) = delete;
  ~PassiveAvrRx() { stop(); }

  bool begin(uint32_t baud, uint32_t now) {
    if (capture_.snapshot().active) { error_ = Error::AlreadyCapturing; return false; }
    if (!Capture::validBaud(baud)) { error_ = Error::BadBaud; return false; }
    // Negative TX means KEEP in this SDK, not detach. Never seize another UART
    // user, and never trust begin(rx18,tx-1) until the stored pins are unassigned.
    if (uart_is_driver_installed(UART_NUM_2) || uart_get_RxPin(2) != -1 ||
        uart_get_TxPin(2) != -1 || uart_get_CtsPin(2) != -1 || uart_get_RtsPin(2) != -1) {
      error_ = Error::UartOwned; return false;
    }
    if (!padIsInput(GPIO_NUM_17)) { error_ = Error::UnsafePins; return false; }
    if (!uartEnableRxInternalPull(2, false) || Serial2.setRxBufferSize(512) != 512) {
      error_ = Error::DriverFailure; return false;
    }
    Serial2.setTxBufferSize(0); // Not a TX-disable guarantee; no TX pad is attached.
    Serial2.onReceiveError([this](hardwareSerial_error_t error) {
      if (error != UART_NO_ERROR) hardwareErrors_.fetch_add(1, std::memory_order_relaxed);
    });
    Serial2.begin(baud, SERIAL_8N1, 18, -1, false);
    ownsDriver_ = true;
    if (!Serial2 || !runningPinsSafe()) {
      error_ = Serial2 ? Error::UnsafePins : Error::DriverFailure;
      endDriver(); return false;
    }
    hardwareErrors_.store(0, std::memory_order_relaxed);
    capture_.begin(baud, now); // Commit/reset the previous capture only on success.
    error_ = Error::None;
    return true;
  }
  void stop() { finish(millis(), StopReason::Requested); }
  void service(uint32_t now) {
    if (!capture_.snapshot().active) return;
    if (!runningPinsSafe()) {
      error_ = Error::UnsafePins; finish(now, StopReason::UnsafePins); return;
    }
    capture_.service(now);
    if (!capture_.snapshot().active) { endDriver(); return; }
    // Poll the current occupancy with a finite per-loop budget. No RX callback
    // decodes packets or calls the application's normal AVR frame handler.
    int pending = Serial2.available();
    if (pending > 64) pending = 64;
    while (pending-- > 0) {
      const int value = Serial2.read();
      if (value < 0) break;
      capture_.record(static_cast<uint8_t>(value));
    }
  }
  Snapshot snapshot() const {
    Snapshot result;
    static_cast<Capture::Snapshot &>(result) = capture_.snapshot();
    result.hardwareErrors = hardwareErrors_.load(std::memory_order_relaxed);
    result.error = error_;
    result.pinsSafe = result.active ? runningPinsSafe() : padIsInput(GPIO_NUM_17);
    return result;
  }
  uint8_t readChunk(uint16_t offset, uint8_t length, uint8_t *output) const {
    return capture_.readChunk(offset, length, output);
  }
 private:
  static bool padIsInput(gpio_num_t pin) {
    gpio_io_config_t state = {};
    return gpio_get_io_config(pin, &state) == ESP_OK && state.ie && !state.oe &&
        !state.oe_ctrl_by_periph && !state.pu && !state.pd;
  }
  static bool runningPinsSafe() {
    return uart_is_driver_installed(UART_NUM_2) && uart_get_RxPin(2) == 18 &&
        uart_get_TxPin(2) == -1 && uart_get_CtsPin(2) == -1 && uart_get_RtsPin(2) == -1 &&
        padIsInput(GPIO_NUM_17) && padIsInput(GPIO_NUM_18);
  }
  void finish(uint32_t now, StopReason reason) {
    capture_.stop(now, reason);
    endDriver();
  }
  void endDriver() {
    if (!ownsDriver_) return;
    Serial2.onReceiveError(nullptr);
    Serial2.end(); // Actual HAL detaches RX/matrix and deletes only this driver.
    ownsDriver_ = false;
  }
  Capture capture_;
  std::atomic<uint32_t> hardwareErrors_{0};
  Error error_ = Error::None;
  bool ownsDriver_ = false;
};
} // namespace PakarS3
#endif
