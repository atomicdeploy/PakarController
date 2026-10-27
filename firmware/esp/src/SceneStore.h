#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Semantic scene, not rendered animation/stream frames or client authority.
struct SceneState {
  uint8_t mode = 1, pattern = 0, brightness = 255, flags = 3;
  uint8_t cells[80] = {};
  uint8_t selectedCell = 1, selectedState = 0, winner = 0;
};
static_assert(sizeof(SceneState) == 87, "Scene state must have no ABI padding");

// Adapted PCController SettingsStore markDirty/service/changed-byte avoidance.
// LittleFS replaces EEPROM: stage, close, exact readback, atomic rename. Never
// truncate the committed scene, and never promote an abandoned pending file.
template <class Filesystem> class SceneStore {
 public:
  static constexpr uint32_t saveDelayMs = 1500, maxDelayMs = 10000;
  static constexpr uint32_t retryDelayMs = 10000;
  bool begin(Filesystem &filesystem, bool mounted, const SceneState &fallback) {
    filesystem_ = mounted ? &filesystem : nullptr;
    desired_ = saved_ = fallback;
    restored_ = savedKnown_ = dirty_ = attempted_ = false;
    writes_ = 0; error_ = mounted ? nullptr : "storage_unavailable";
    if (!mounted || !filesystem.exists(path())) return false;
    Record record{};
    if (!read(path(), record)) { error_ = "scene_damaged"; return false; }
    desired_ = saved_ = record.state; restored_ = savedKnown_ = true;
    return true;
  }
  const SceneState &state() const { return desired_; }
  bool restored() const { return restored_; }
  bool pending() const { return dirty_; }
  bool saved() const { return savedKnown_ && !dirty_; }
  uint32_t writes() const { return writes_; }
  const char *error() const { return error_; }
  void observe(const SceneState &state, uint32_t now) {
    if (!valid(state) || !memcmp(&state, &desired_, sizeof(state))) return;
    if (!dirty_) firstChangeAt_ = now;
    desired_ = state; changedAt_ = now;
    dirty_ = !savedKnown_ || memcmp(&desired_, &saved_, sizeof(state));
  }
  bool service(uint32_t now, bool allowWrite = true, bool flush = false) {
    if (flush && !savedKnown_ && !dirty_) {
      dirty_ = true; firstChangeAt_ = changedAt_ = now;
    }
    if (!dirty_ || !allowWrite) return false;
    if (!flush && uint32_t(now - changedAt_) < saveDelayMs &&
        uint32_t(now - firstChangeAt_) < maxDelayMs) return false;
    // A failing filesystem must not turn every loop into a write attempt.
    if (attempted_ && uint32_t(now - attemptedAt_) < retryDelayMs) return false;
    attempted_ = true; attemptedAt_ = now;
    if (!filesystem_) { error_ = "storage_unavailable"; return false; }
    Record next{}; next.magic = 0x53434b50UL; next.layout = 1;
    next.state = desired_; next.check = checksum(next);
    auto file = filesystem_->open(pendingPath(), "w");
    if (!file) { error_ = "scene_stage_failed"; return false; }
    const bool written = file.write(reinterpret_cast<const uint8_t *>(&next), sizeof(next)) == sizeof(next);
    file.close();
    Record verify{};
    if (!written || !read(pendingPath(), verify) || memcmp(&verify, &next, sizeof(next))) {
      error_ = "scene_verify_failed"; return false;
    }
    if (!filesystem_->rename(pendingPath(), path())) { error_ = "scene_commit_failed"; return false; }
    saved_ = desired_; savedKnown_ = true; dirty_ = false; ++writes_;
    error_ = nullptr; attempted_ = false;
    return true;
  }
  static const char *path() { return "/scene.bin"; }
  static const char *pendingPath() { return "/scene.pending"; }
 private:
  struct Record {
    uint32_t magic;
    uint8_t layout;
    SceneState state;
    uint32_t check;
  };
  static_assert(sizeof(Record) == 96 && offsetof(Record, check) == 92,
                "Private scene record layout changed");
  static uint32_t checksum(const Record &record) {
    uint32_t result = 2166136261UL;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < offsetof(Record, check); ++i) result = (result ^ bytes[i]) * 16777619UL;
    return result;
  }
  static bool valid(const SceneState &state) {
    if (state.mode < 1 || state.mode > 3 || state.pattern > 3 || !state.brightness ||
        (state.flags & ~7U) || state.selectedCell < 1 || state.selectedCell > 80 ||
        state.selectedState > 3 || state.winner > 9) return false;
    for (uint8_t cell : state.cells) if (cell > 3) return false;
    return true;
  }
  bool read(const char *name, Record &record) {
    auto file = filesystem_->open(name, "r");
    const bool complete = file && file.size() == sizeof(record) &&
        file.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record);
    if (file) file.close();
    return complete && record.magic == 0x53434b50UL && record.layout == 1 &&
        record.check == checksum(record) && valid(record.state);
  }
  Filesystem *filesystem_ = nullptr;
  SceneState desired_{}, saved_{};
  uint32_t firstChangeAt_ = 0, changedAt_ = 0, attemptedAt_ = 0, writes_ = 0;
  bool restored_ = false, savedKnown_ = false, dirty_ = false, attempted_ = false;
  const char *error_ = nullptr;
};
