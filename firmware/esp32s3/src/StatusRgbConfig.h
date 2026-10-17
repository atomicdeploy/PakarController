#pragma once
#include <FS.h>
#include <stddef.h>
#include <string.h>
#include "StatusRgb.h"

namespace PakarS3 {
// Persistent OWNER of onboard RGB configuration. Adapted from the actual
// NetworkConfig.cpp / AccessConfig.cpp pending-file + readback + rename path.
// Validation precedes writes; active configuration publishes only after commit.
// Renderer configuration is a separate local-driver transaction: root validates
// the candidate, saves it, then calls StatusRgb::configure with this readback.
class StatusRgbConfig {
 public:
  using Configuration = StatusRgb::Configuration;
  static bool validate(const Configuration &value) {
    if (value.pin == -1) return !value.enabled && !value.pinConfirmed;
    return StatusRgb::supportedPin(value.pin) &&
        (!value.enabled || value.pinConfirmed);
  }
  bool begin(fs::FS &filesystem, const Configuration &fallback = Configuration()) {
    filesystem_ = &filesystem;
    persisted_ = false;
    active_ = validate(fallback) ? fallback : Configuration();
    error_ = validate(fallback) ? "" : "invalid RGB build default; disabled fallback loaded";
    if (!filesystem.exists(kPath)) return validate(fallback);
    Record stored{};
    if (!read(kPath, stored) || !valid(stored)) {
      error_ = "RGB settings checksum/layout is invalid; safe build default retained";
      return false;
    }
    active_ = decode(stored);
    persisted_ = true; error_ = "";
    return true;
  }
  bool save(const Configuration &candidate) {
    if (!validate(candidate)) { error_ = "RGB pin must be explicitly confirmed38/48 before enabling"; return false; }
    if (!filesystem_) { error_ = "RGB settings filesystem unavailable"; return false; }
    const Record next = encode(candidate);
    Record previous{};
    if (read(kPath, previous) && memcmp(&previous, &next, sizeof(next)) == 0) {
      active_ = candidate; persisted_ = true; error_ = "";
      return true; // No write/rename, even on the first save after a reboot.
    }
    File pending = filesystem_->open(kPending, "w");
    if (!pending) { error_ = "cannot stage RGB settings; previous configuration retained"; return false; }
    const bool written = pending.write(reinterpret_cast<const uint8_t *>(&next), sizeof(next)) == sizeof(next);
    pending.close();
    Record verified{};
    if (!written || !read(kPending, verified) || !valid(verified) ||
        memcmp(&verified, &next, sizeof(next)) != 0 ||
        !filesystem_->rename(kPending, kPath)) {
      error_ = "cannot commit RGB settings; previous configuration retained";
      return false;
    }
    active_ = candidate;
    persisted_ = true; error_ = "";
    return true;
  }
  const Configuration &configuration() const { return active_; }
  bool persisted() const { return persisted_; }
  const char *lastError() const { return error_; }

 private:
  struct __attribute__((packed)) Record {
    uint32_t magic;
    uint8_t layout; // Private fixed flash-record validation, never an API selector.
    int8_t pin;
    uint8_t enabled, pinConfirmed, brightness;
    uint32_t checksum;
  };
  static constexpr const char *kPath = "/status-rgb.bin";
  static constexpr const char *kPending = "/status-rgb.pending";
  static uint32_t checksum(const Record &record) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t value = 2166136261UL;
    for (size_t index = 0; index < offsetof(Record, checksum); ++index)
      value = (value ^ bytes[index]) * 16777619UL;
    return value;
  }
  static Record encode(const Configuration &value) {
    Record result{0x504B5247UL, 1, value.pin, uint8_t(value.enabled),
        uint8_t(value.pinConfirmed), value.brightness, 0};
    result.checksum = checksum(result);
    return result;
  }
  static Configuration decode(const Record &record) {
    Configuration result;
    result.pin = record.pin; result.enabled = record.enabled;
    result.pinConfirmed = record.pinConfirmed; result.brightness = record.brightness;
    return result;
  }
  static bool valid(const Record &record) {
    return record.magic == 0x504B5247UL && record.layout == 1 &&
        record.enabled <= 1 && record.pinConfirmed <= 1 &&
        record.checksum == checksum(record) && validate(decode(record));
  }
  bool read(const char *path, Record &record) {
    File file = filesystem_->open(path, "r");
    const bool complete = file && file.size() == sizeof(record) &&
        file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record);
    if (file) file.close();
    return complete;
  }
  Configuration active_;
  fs::FS *filesystem_ = nullptr;
  bool persisted_ = false;
  const char *error_ = "";
};
} // namespace PakarS3
