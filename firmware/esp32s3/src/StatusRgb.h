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
    Off, Boot, Connecting, Ready, AccessPoint, PeerLost, Updating, Warning, Fault,
    LocalReady, AccessPointNetwork, Shutdown, Success
  };
  // Preserve the existing static/breathe IDs and the donor's flash, cycle and
  // one-shot transition vocabulary. Pulse/double-blink are additive patterns.
  enum class Effect : uint8_t { Static, Breathe, Blink, Cycle, Fade, Pulse, DoubleBlink };
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
    uint8_t effect = 0;
    uint8_t progress = 255; // Unknown progress is not fabricated as zero.
    uint32_t revision = 0, updatedAt = 0;
  };
  struct Observations {
    bool fault=false, updating=false, booting=false;
    bool shutdown=false, success=false, warning=false;
    bool stationEnabled=false, networkReady=false, accessPoint=false;
    bool peerRequired=false, peerOnline=false;
  };
  static State selectState(const Observations &value) {
    if(value.fault)return State::Fault;
    if(value.shutdown)return State::Shutdown;
    if(value.updating)return State::Updating;
    if(value.booting)return State::Boot;
    if(value.warning)return State::Warning;
    if(value.peerRequired&&!value.peerOnline)return State::PeerLost;
    if(value.success)return State::Success;
    if(value.stationEnabled&&!value.networkReady)return State::Connecting;
    if(value.networkReady&&value.accessPoint)return State::AccessPointNetwork;
    if(value.networkReady)return State::Ready;
    // Intentionally offline USB operation is healthy, not a network warning.
    return value.accessPoint?State::AccessPoint:State::LocalReady;
  }

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
      case State::LocalReady: return "local-ready";
      case State::AccessPointNetwork: return "access-point-network";
      case State::Shutdown: return "shutdown";
      case State::Success: return "success";
    }
    return "unknown";
  }
  static const char *effectName(uint8_t effect) {
    switch (static_cast<Effect>(effect)) {
      case Effect::Static: return "static";
      case Effect::Breathe: return "breathing";
      case Effect::Blink: return "blinking";
      case Effect::Cycle: return "color-fade";
      case Effect::Fade: return "fade-out";
      case Effect::Pulse: return "pulse";
      case Effect::DoubleBlink: return "double-blink";
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
    bool nextPinBlack=false;
    if (configured_ && config_.enabled && config_.pinConfirmed &&
        (!next.enabled || next.pin != config_.pin)) {
      if(!writer(config_.pin,0,0,0))return false;
      nextPinBlack=next.pin==config_.pin;
    }
    if(!next.enabled&&next.pinConfirmed&&supportedPin(next.pin)&&!nextPinBlack&&
        !writer(next.pin,0,0,0))return false;
    config_ = next;
    configured_ = true;
    failed_ = false; // Explicit reconfiguration starts a fresh driver attempt.
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
    if (failed_ && uint32_t(now-lastServiceAt_) < 250U) return false;
    if (!dirty_ && uint32_t(now - lastServiceAt_) < 16U) return false;
    lastServiceAt_ = now;
    uint8_t rgb[3] = {};
    uint16_t period = 0;
    Effect effect = Effect::Static;
    switch (state_) {
      case State::Off: break;
      case State::Boot: rgb[2] = 255; period = 1800; effect = Effect::Breathe; break;
      case State::Connecting: rgb[0] = 255; rgb[1] = 128; period = 1400; effect = Effect::Pulse; break;
      case State::Ready: rgb[1] = 255; rgb[2] = 40; break;
      case State::LocalReady: rgb[1] = 130; rgb[2] = 180; break;
      case State::AccessPoint: rgb[1] = 200; rgb[2] = 255; period = 2600; effect = Effect::Breathe; break;
      case State::AccessPointNetwork: rgb[1] = 255; rgb[2] = 40; period = 3200; effect = Effect::Cycle; break;
      case State::PeerLost: rgb[0] = 255; rgb[1] = 90; period = 2100; effect = Effect::DoubleBlink; break;
      case State::Updating: rgb[0] = 165; rgb[2] = 255; period = 1100; effect = Effect::Pulse; break;
      case State::Warning: rgb[0] = 255; rgb[1] = 145; period = 1800; effect = Effect::Blink; break;
      case State::Fault: rgb[0] = 255; period = 1200; effect = Effect::DoubleBlink; break;
      case State::Shutdown: rgb[0] = 165; rgb[2] = 255; effect = Effect::Fade; break;
      case State::Success: rgb[1] = 255; rgb[2] = 40; period = 800; effect = Effect::Pulse; break;
    }
    uint8_t level = config_.brightness;
    const uint32_t age = now-stateAt_;
    if (effect == Effect::Breathe || effect == Effect::Cycle) {
      // PCController's absolute elapsed/period evaluation, eased triangle.
      // Late service advances to the current phase instead of slowing time.
      const uint16_t phase = age % period * 512UL / period;
      const uint8_t triangle = phase < 256 ? phase : 511 - phase;
      if (effect == Effect::Cycle) {
        // Both interfaces available: slowly fade healthy green to AP cyan.
        rgb[1] = interpolate(255,200,ease(triangle));
        rgb[2] = interpolate(40,255,ease(triangle));
      } else {
        const uint8_t floor = config_.brightness / 5U;
        level = floor + scale(config_.brightness - floor, ease(triangle));
      }
    } else if (effect == Effect::Pulse) {
      const uint16_t phase = age % period;
      const uint16_t width = 400;
      const uint8_t pulse = phase >= width ? 0 : ease(255U-phase*255UL/width);
      const uint8_t floor = state_ == State::Updating && progress_ <= 100
          ? uint16_t(config_.brightness)*progress_/100U : config_.brightness/8U;
      level = floor + scale(config_.brightness-floor,pulse);
    } else if (effect == Effect::Blink) {
      level = age % period < 350U ? config_.brightness : 0;
    } else if (effect == Effect::DoubleBlink) {
      const uint16_t phase = age % period;
      level = (phase < 140U || (phase >= 280U && phase < 420U)) ? config_.brightness : 0;
    } else if (effect == Effect::Fade) {
      // One shot, held at actual black until the lifecycle advances/reboots.
      level = age >= 700U ? 0 : scale(config_.brightness,ease(255U-age*255UL/700U));
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
    if (!writer(config_.pin, rgb[0], rgb[1], rgb[2])) { failed_ = true; return false; }
    failed_ = false;
    output_.red = rgb[0]; output_.green = rgb[1]; output_.blue = rgb[2];
    output_.condition = state_;
    output_.effect = static_cast<uint8_t>(effect);
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
  bool configured_ = false, dirty_ = true, failed_ = false;
  uint8_t from_[3] = {}, progress_ = 255;
  uint32_t stateAt_ = 0, transitionAt_ = 0, lastServiceAt_ = 0;
};
} // namespace PakarS3
