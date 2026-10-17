#pragma once
#include <stdint.h>

namespace PakarS3 {

// Onboard status pixel, never the GPIO4 matrix. The application supplies the
// reviewed board pin and its RGB writer; this class does not probe GPIOs.
// Renderer arithmetic is adapted from PCController Project/StatusLedMath.h
// and StatusLedController.cpp (scale, interpolate, elapsed/period service).
// Pealayer src/ui/status_bar.rs mirrors these already-scaled output bytes
// directly: clients must NOT apply brightness again or synthesize animation.
class StatusRgb {
 public:
  enum class State : uint8_t {
    Off, Boot, Connecting, Ready, AccessPoint, PeerLost, Updating, Warning, Fault
  };
  struct Configuration {
    int8_t pin = -1;
    bool enabled = false;
    bool pinConfirmed = false;
    uint8_t brightness = 64;
  };
  struct Snapshot {
    bool available = false;
    bool enabled = false;
    bool haveOutput = false;
    int8_t pin = -1;
    uint8_t red = 0, green = 0, blue = 0;
    uint8_t brightness = 0;
    State condition = State::Off;
    uint8_t effect = 0; // PCController presentation: 0 static, 1 breathe.
    uint8_t progress = 255; // Unknown progress is not fabricated as zero.
    uint32_t revision = 0, updatedAt = 0;
  };

  // Common DevKit RGB positions ONLY, and neither is selected automatically.
  // A different board needs an explicit reviewed whitelist addition, not a
  // persisted arbitrary GPIO that could drive flash, UART, USB or AVR RESET.
  static bool supportedPin(int pin) { return pin == 38 || pin == 48; }
  static const char *name(State state) {
    switch (state) {
      case State::Off: return "off";
      case State::Boot: return "boot";
      case State::Connecting: return "connecting";
      case State::Ready: return "ready";
      case State::AccessPoint: return "access-point";
      case State::PeerLost: return "peer-lost";
      case State::Updating: return "updating";
      case State::Warning: return "warning";
      case State::Fault: return "fault";
    }
    return "unknown";
  }

  // Writer(pin,r,g,b) returns true only after the local LED driver accepted
  // the frame. That is output-driver evidence, not optical verification.
  template<class Writer>
  bool configure(const Configuration &next, uint32_t now, Writer writer) {
    if (next.enabled && (!next.pinConfirmed || !supportedPin(next.pin)))
      return false;
    if (next.pin != -1 && !supportedPin(next.pin)) return false;
    if (configured_ && config_.pin == next.pin &&
        config_.enabled == next.enabled &&
        config_.pinConfirmed == next.pinConfirmed &&
        config_.brightness == next.brightness) return true;
    // Extinguish an old confirmed pin before reassigning or disabling it.
    if (configured_ && config_.enabled && config_.pinConfirmed &&
        (!next.enabled || next.pin != config_.pin) &&
        !writer(config_.pin, 0, 0, 0)) return false;
    config_ = next;
    configured_ = true;
    output_.available = next.pinConfirmed && supportedPin(next.pin);
    output_.enabled = next.enabled;
    output_.pin = next.pin;
    output_.brightness = next.brightness;
    output_.haveOutput = false;
    dirty_ = true;
    transitionAt_ = now;
    from_[0] = output_.red; from_[1] = output_.green; from_[2] = output_.blue;
    if (!next.enabled) {
      output_.red = output_.green = output_.blue = 0;
      output_.condition = State::Off;
      output_.effect = 0;
      // Disabled/unidentified does not imply a physically acknowledged pixel.
      output_.haveOutput = output_.available;
      record(now);
      return true;
    }
    service(now, writer);
    return output_.haveOutput;
  }

  void setState(State state, uint32_t now) {
    if (state_ == state) return; // Exact repeats retain the animation phase.
    from_[0] = output_.red; from_[1] = output_.green; from_[2] = output_.blue;
    state_ = state;
    stateAt_ = transitionAt_ = now;
    dirty_ = true;
  }
  void setProgress(uint8_t percent) {
    const uint8_t next = percent <= 100 ? percent : 255;
    if (next != progress_) { progress_ = next; dirty_ = true; }
  }
  const Snapshot &snapshot() const { return output_; }
  const Configuration &configuration() const { return config_; }

  template<class Writer>
  bool service(uint32_t now, Writer writer) {
    if (!configured_ || !config_.enabled || !config_.pinConfirmed) return false;
    // At most one output per 16ms service interval, no delay or catch-up loop.
    // This is the ONE status pixel cadence, never a matrix/stream FPS limit.
    if (!dirty_ && uint32_t(now - lastServiceAt_) < 16U) return false;
    lastServiceAt_ = now;
    uint8_t rgb[3] = {};
    uint16_t period = 0;
    switch (state_) {
      case State::Off: break;
      case State::Boot: rgb[2] = 255; period = 1800; break;
      case State::Connecting: rgb[0] = 255; rgb[1] = 128; period = 1400; break;
      case State::Ready: rgb[1] = 255; rgb[2] = 40; break;
      case State::AccessPoint: rgb[1] = 200; rgb[2] = 255; period = 2600; break;
      case State::PeerLost: rgb[0] = 255; rgb[1] = 90; period = 2100; break;
      case State::Updating: rgb[0] = 165; rgb[2] = 255; period = 1100; break;
      case State::Warning: rgb[0] = 255; rgb[1] = 145; period = 1800; break;
      case State::Fault: rgb[0] = 255; period = 1000; break;
    }
    uint8_t level = config_.brightness;
    if (period != 0) {
      // PCController's absolute elapsed/period evaluation, eased triangle.
      // Late service advances to the current phase instead of slowing time.
      const uint16_t phase = uint32_t(now - stateAt_) % period * 512UL / period;
      const uint8_t triangle = phase < 256 ? phase : 511 - phase;
      const uint8_t floor = state_ == State::Updating && progress_ <= 100
          ? uint16_t(config_.brightness) * progress_ / 100U
          : config_.brightness / 5U;
      level = floor + scale(config_.brightness - floor, ease(triangle));
    }
    const uint32_t elapsed = now - transitionAt_;
    const uint8_t transition = elapsed >= 300 ? 255 : elapsed * 255U / 300U;
    for (uint8_t channel = 0; channel < 3; ++channel) {
      const uint8_t target = scale(rgb[channel], level);
      rgb[channel] = elapsed >= 300 ? target
          : interpolate(from_[channel], target, ease(transition));
    }
    const bool changed = dirty_ || !output_.haveOutput ||
        output_.red != rgb[0] || output_.green != rgb[1] || output_.blue != rgb[2];
    if (!changed) return false;
    if (!writer(config_.pin, rgb[0], rgb[1], rgb[2])) return false;
    output_.red = rgb[0]; output_.green = rgb[1]; output_.blue = rgb[2];
    output_.condition = state_;
    output_.effect = period ? 1 : 0;
    output_.progress = progress_;
    output_.haveOutput = true;
    dirty_ = false;
    record(now);
    return true;
  }

 private:
  // Adapted verbatim arithmetic from PCController's shared StatusLedMath.
  static uint8_t scale(uint8_t value, uint8_t level) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) *
        (static_cast<uint16_t>(level) + 1U)) >> 8);
  }
  static uint8_t interpolate(uint8_t from, uint8_t to, uint8_t phase) {
    if (to >= from) return from + (uint16_t(to - from) * phase) / 256U;
    return from - (uint16_t(from - to) * phase) / 256U;
  }
  static uint8_t ease(uint8_t phase) {
    const uint32_t square = uint32_t(phase) * phase;
    return (square * (765U - 2U * phase) + 32512U) / 65025U;
  }
  void record(uint32_t now) {
    ++output_.revision;
    if (output_.revision == 0) ++output_.revision;
    output_.updatedAt = now;
  }
  Configuration config_;
  Snapshot output_;
  State state_ = State::Boot;
  bool configured_ = false, dirty_ = true;
  uint8_t from_[3] = {}, progress_ = 255;
  uint32_t stateAt_ = 0, transitionAt_ = 0, lastServiceAt_ = 0;
};
} // namespace PakarS3
