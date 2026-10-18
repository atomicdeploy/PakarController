#pragma once
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <interrupts.h>
#endif

// PU850 AsyncOTA's measured transfer counters, adapted to PCController's
// UPDATE-PROGRESS.md: bytes describe one stage, never whole-update success.
// One serialized async writer publishes to the loop owner without a String,
// UART call, flash read or display transaction in the TCP callback.
namespace PakarUpdate {
enum class Phase : uint32_t { Idle, Receiving, Verifying, Rebooting, Failed, Aborted };
inline const char *name(Phase phase) {
  switch (phase) {
    case Phase::Receiving: return "receiving";
    case Phase::Verifying: return "verifying";
    case Phase::Rebooting: return "ready-to-reboot";
    case Phase::Failed: return "failed";
    case Phase::Aborted: return "aborted";
    default: return "idle";
  }
}
struct Observation {
  Phase phase = Phase::Idle;
  uint32_t received = 0, expected = 0;
  bool known() const { return expected && received <= expected; }
  uint8_t percent() const {
    return known() ? static_cast<uint8_t>(static_cast<uint64_t>(received) * 100 / expected) : 0;
  }
};
class Progress {
 public:
  void begin(uint32_t expected) { publish({Phase::Receiving, 0, expected}); }
  void advance(uint32_t received) { auto next = snapshot(); next.received = received; publish(next); }
  void phase(Phase phase) { auto next = snapshot(); next.phase = phase; publish(next); }
  Observation snapshot() const {
#if defined(ARDUINO_ARCH_ESP8266)
    esp8266::InterruptLock lock;
    return observation_;
#else
    for (;;) {
      const auto before = revision_.load();
      if (before & 1U) continue;
      Observation value{static_cast<Phase>(phase_.load()), received_.load(), expected_.load()};
      if (before == revision_.load()) return value;
    }
#endif
  }
 private:
  void publish(Observation value) {
#if defined(ARDUINO_ARCH_ESP8266)
    esp8266::InterruptLock lock;
    observation_=value;
#else
    revision_.fetch_add(1);
    phase_.store(static_cast<uint32_t>(value.phase));
    received_.store(value.received); expected_.store(value.expected);
    revision_.fetch_add(1);
#endif
  }
#if defined(ARDUINO_ARCH_ESP8266)
  Observation observation_;
#else
  std::atomic<uint32_t> revision_{0}, phase_{0}, received_{0}, expected_{0};
#endif
};
// Exact raw file size, NOT multipart Content-Length or OTA partition capacity.
inline bool imageSize(const char *text, uint32_t capacity, uint32_t &out) {
  out = 0;
  if (!text || !*text) return false;
  uint64_t value = 0;
  for (unsigned i = 0; text[i]; ++i) {
    if (i >= 10 || text[i] < '0' || text[i] > '9') return false;
    value = value * 10 + text[i] - '0';
    if (value >= capacity) return false;
  }
  if (!value) return false;
  out = static_cast<uint32_t>(value); return true;
}
// A finite host-panel lease. Missing/stale TM1637 ACK or AVR programming inhibits
// delivery; a successful serial enqueue is NOT display presentation evidence.
class Panel {
 public:
  bool next(uint32_t now, bool active, bool available, const char text[5], uint8_t payload[12]) {
    if (!active) { started_ = false; return false; }
    if (!available || (started_ && static_cast<uint32_t>(now - sentAt_) < 250U)) return false;
    bool same = started_;
    for (unsigned i = 0; i < 4; ++i) same = same && text[i] == last_[i];
    if (same && static_cast<uint32_t>(now - sentAt_) < 1000U) return false;
    // Existing kDisplayText scheduled segment contract: one pass, 1200ms hold.
    const uint8_t prefix[8] = {5, 4, 1, 4, 0, 0xB0, 0x04, 0};
    for (unsigned i = 0; i < 8; ++i) payload[i] = prefix[i];
    for (unsigned i = 0; i < 4; ++i) payload[8+i] = last_[i] = text[i];
    started_ = true; sentAt_ = now; return true;
  }
 private:
  bool started_ = false;
  uint32_t sentAt_ = 0;
  char last_[4] = {};
};
inline void panelText(const Observation &value, char text[5]) {
  if (value.phase == Phase::Failed || value.phase == Phase::Aborted) snprintf(text, 5, "Err ");
  else if (value.phase == Phase::Rebooting) snprintf(text, 5, "boot");
  else if (value.phase == Phase::Verifying) snprintf(text, 5, "CHEC");
  else if (value.known()) snprintf(text, 5, "P%03u", value.percent());
  else snprintf(text, 5, "UPdt");
}
}  // namespace PakarUpdate
