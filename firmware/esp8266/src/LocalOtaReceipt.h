#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>

// PU850 AsyncOTA's Update.end/reboot boundary is NOT completion. Adapt the
// PCController measured-stage/postboot/no-replay contract with SceneStore's
// pending/closed/exact-readback/atomic-rename persistence. No updater is owned
// here, and neither begin nor a notice can initiate another flash operation.
template <class Filesystem> class LocalOtaReceipt {
 public:
  enum class Notice : uint8_t { None, Complete, Failed };
  static constexpr size_t maximumRecordBytes = 768;
  static const char *path() { return "/local-ota.json"; }
  static const char *pendingPath() { return "/local-ota.pending"; }

  // Call once after filesystem mount and running-image metadata collection.
  // A dangling .pending is never promoted, including after a failed rename.
  bool begin(Filesystem &filesystem, bool mounted, const char *currentBoot,
             uint32_t runningBytes, const char *runningMd5,
             bool runningIntegrity) {
    filesystem_ = mounted ? &filesystem : nullptr;
    record_ = Record{}; known_ = installed_ = false; error_ = nullptr; writes_ = 0;
    if (!mounted) { error_ = "storage_unavailable"; return false; }
    if (!identity(currentBoot)) { error_ = "boot_identity_unavailable"; return false; }
    currentBoot_ = currentBoot;
    if (!filesystem.exists(path())) return false;
    Record loaded;
    if (!read(path(), loaded)) { error_ = "local_ota_receipt_damaged"; return false; }
    record_ = loaded; known_ = true;
    if (record_.state == State::AwaitingReboot) {
      if (record_.sourceBoot == currentBoot_) return true;
      Record verified = record_;
      verified.installedBoot = currentBoot_;
      verified.observedBytes = runningBytes;
      verified.observedMd5 = md5(runningMd5) ? runningMd5 : "";
      verified.integrity = runningIntegrity;
      const char *failure = !runningIntegrity ? "running_integrity_failed" :
          !record_.md5.length() ? "expected_digest_unavailable" :
          runningBytes != record_.bytes ? "running_size_mismatch" :
          !md5(runningMd5) ? "running_digest_unavailable" :
          record_.md5 != runningMd5 ? "running_digest_mismatch" : nullptr;
      verified.state = failure ? State::Failed : State::Complete;
      verified.failure = failure ? failure : "";
      // If persistence fails, retain the committed awaiting state. A caller
      // must not present a non-durable terminal state or play a success cue.
      if (!commit(verified)) return false;
    }
    installed_ = record_.state != State::AwaitingReboot && runningIntegrity &&
        record_.bytes == runningBytes && record_.md5.length() && md5(runningMd5) &&
        record_.md5 == runningMd5;
    return true;
  }

  // Call ONLY after a successful Update.end(true), with its actual computed
  // digest, never just a caller-supplied header. A missing digest is retained
  // as unknown and cannot produce a verified completion on the next boot.
  bool awaitingReboot(const char *sourceBoot, uint32_t id, uint32_t bytes,
                      const char *computedMd5 = "") {
    if (!filesystem_ || !identity(sourceBoot) || currentBoot_ != sourceBoot || !id || bytes < 16 ||
        bytes > 16UL * 1024UL * 1024UL || !computedMd5 ||
        (*computedMd5 && !md5(computedMd5))) {
      error_ = filesystem_ ? "invalid_local_ota_receipt" : "storage_unavailable";
      return false;
    }
    if (known_ && record_.id == id && record_.sourceBoot == sourceBoot) {
      if (record_.bytes == bytes && record_.md5 == computedMd5 &&
          record_.state == State::AwaitingReboot) return true;
      error_ = "local_ota_identity_consumed"; return false;
    }
    Record next;
    next.id = id; next.sourceBoot = sourceBoot; next.bytes = bytes;
    next.md5 = computedMd5; next.state = State::AwaitingReboot;
    if (!commit(next)) return false;
    installed_ = false;
    return true;
  }

  // Write-ahead consumption provides AT MOST ONCE presentation. A reset after
  // this commit but before sound may omit that cue; it must never repeat it.
  // The caller retains responsibility for silent preference and actual output.
  Notice takeNotice() {
    if (!known_ || record_.noticeConsumed || pending() ||
        (record_.state == State::Complete && !completionVerified())) return Notice::None;
    Record next = record_; next.noticeConsumed = true;
    const auto notice = record_.state == State::Complete ? Notice::Complete : Notice::Failed;
    return commit(next) ? notice : Notice::None;
  }

  bool known() const { return known_; }
  bool pending() const { return known_ && record_.state == State::AwaitingReboot; }
  bool installed() const { return installed_; }
  bool completionVerified() const { return known_ && installed_ && record_.state == State::Complete; }
  bool noticeConsumed() const { return known_ && record_.noticeConsumed; }
  uint32_t uploadId() const { return known_ ? record_.id : 0; }
  const String &sourceBoot() const { return record_.sourceBoot; }
  const String &installedBoot() const { return record_.installedBoot; }
  const String &expectedMd5() const { return record_.md5; }
  uint32_t expectedBytes() const { return known_ ? record_.bytes : 0; }
  uint32_t writes() const { return writes_; }
  const char *error() const { return error_ ? error_ : record_.failure.c_str(); }
  const char *phase() const {
    return !known_ ? "idle" : record_.state == State::AwaitingReboot ? "awaiting_reboot" :
        record_.state == State::Complete ? "complete" : "failed";
  }
  String json() const {
    JsonDocument d;
    d["job_id"] = uploadId(); d["phase"] = phase();
    d["source_boot_id"] = record_.sourceBoot; d["installed_boot_id"] = record_.installedBoot;
    d["expected_bytes"] = expectedBytes(); d["expected_md5"] = record_.md5;
    d["observed_bytes"] = record_.observedBytes; d["observed_md5"] = record_.observedMd5;
    d["integrity_verified"] = record_.integrity;
    d["pending"] = pending(); d["installed"] = installed_;
    d["completion_verified"] = completionVerified();
    d["notice_consumed"] = noticeConsumed(); d["automatic_retry"] = false;
    d["error"] = error();
    String out; serializeJson(d, out); return out;
  }

 private:
  enum class State : uint8_t { AwaitingReboot = 1, Complete = 2, Failed = 3 };
  struct Record {
    uint32_t id = 0, bytes = 0, observedBytes = 0;
    String sourceBoot, installedBoot, md5, observedMd5, failure;
    State state = State::AwaitingReboot;
    bool integrity = false, noticeConsumed = false;
  };
  static bool identity(const char *text) {
    if (!text) return false;
    size_t n = 0;
    for (; n <= 80 && text[n]; ++n)
      if (static_cast<uint8_t>(text[n]) < 33 || static_cast<uint8_t>(text[n]) > 126) return false;
    return n && n <= 80;
  }
  static bool md5(const char *text) {
    if (!text) return false;
    for (size_t i = 0; i < 32; ++i)
      if (!text[i] || !((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return !text[32];
  }
  static void fields(JsonDocument &d, const Record &r) {
    d["magic"] = "PakarLocalOTA";
    d["id"] = r.id; d["state"] = static_cast<uint8_t>(r.state);
    d["source_boot"] = r.sourceBoot; d["bytes"] = r.bytes; d["md5"] = r.md5;
    d["installed_boot"] = r.installedBoot; d["observed_bytes"] = r.observedBytes;
    d["observed_md5"] = r.observedMd5; d["integrity"] = r.integrity;
    d["notice_consumed"] = r.noticeConsumed; d["failure"] = r.failure;
  }
  static uint32_t checksum(const Record &r) {
    JsonDocument d; fields(d, r); String canonical; serializeJson(d, canonical);
    uint32_t value = 2166136261UL;
    for (size_t i = 0; i < canonical.length(); ++i)
      value = (value ^ static_cast<uint8_t>(canonical[i])) * 16777619UL;
    return value;
  }
  static String encoded(const Record &r) {
    JsonDocument d; fields(d, r); d["checksum"] = checksum(r);
    String out; serializeJson(d, out); return out;
  }
  bool read(const char *name, Record &r, String *raw = nullptr) {
    auto file = filesystem_->open(name, "r");
    if (!file || !file.size() || file.size() > maximumRecordBytes) {
      if (file) file.close();
      return false;
    }
    char bytes[maximumRecordBytes + 1] = {};
    const size_t size = file.size();
    const bool complete = file.read(reinterpret_cast<uint8_t *>(bytes), size) == size;
    file.close();
    if (!complete) return false;
    // Explicit bounded input catches embedded NULs/trailing garbage too.
    for (size_t i = 0; i < size; ++i) if (!bytes[i]) return false;
    JsonDocument d;
    if (deserializeJson(d, bytes, size) || !d.is<JsonObject>() ||
        !d["magic"].is<const char *>() || strcmp(d["magic"], "PakarLocalOTA") ||
        !d["id"].is<uint32_t>() || !d["id"].as<uint32_t>() ||
        !d["state"].is<uint8_t>() || !d["bytes"].is<uint32_t>() ||
        d["bytes"].as<uint32_t>() < 16 || d["bytes"].as<uint32_t>() > 16UL*1024UL*1024UL ||
        !d["source_boot"].is<const char *>() || !identity(d["source_boot"]) ||
        !d["md5"].is<const char *>() ||
        !d["installed_boot"].is<const char *>() ||
        !d["observed_bytes"].is<uint32_t>() ||
        !d["observed_md5"].is<const char *>() ||
        !d["integrity"].is<bool>() || !d["notice_consumed"].is<bool>() ||
        !d["failure"].is<const char *>() || !d["checksum"].is<uint32_t>()) return false;
    const auto state = d["state"].as<uint8_t>();
    if (state < 1 || state > 3) return false;
    r.id = d["id"]; r.bytes = d["bytes"]; r.state = static_cast<State>(state);
    r.sourceBoot = d["source_boot"].as<String>(); r.md5 = d["md5"].as<String>();
    r.installedBoot = d["installed_boot"].as<String>();
    r.observedBytes = d["observed_bytes"]; r.observedMd5 = d["observed_md5"].as<String>();
    r.integrity = d["integrity"]; r.noticeConsumed = d["notice_consumed"];
    r.failure = d["failure"].as<String>();
    const bool waiting = r.state == State::AwaitingReboot;
    if ((r.md5.length() && !md5(r.md5.c_str())) ||
        (r.observedMd5.length() && !md5(r.observedMd5.c_str())) ||
        (waiting && (r.installedBoot.length() || r.observedBytes || r.observedMd5.length() ||
                     r.integrity || r.noticeConsumed || r.failure.length())) ||
        (!waiting && (!identity(r.installedBoot.c_str()) || r.installedBoot == r.sourceBoot)) ||
        (r.state == State::Complete && (!r.md5.length() || r.md5 != r.observedMd5 ||
           !r.integrity || r.bytes != r.observedBytes || r.failure.length())) ||
        (r.state == State::Failed && (!r.failure.length() || r.failure.length() > 64)) ||
        d["checksum"].as<uint32_t>() != checksum(r)) return false;
    const String canonical = encoded(r);
    if (canonical.length() != size || memcmp(canonical.c_str(), bytes, size)) return false;
    if (raw) *raw = bytes;
    return true;
  }
  bool commit(const Record &next) {
    if (!filesystem_) { error_ = "storage_unavailable"; return false; }
    const String bytes = encoded(next);
    if (!bytes.length() || bytes.length() > maximumRecordBytes) {
      error_ = "local_ota_receipt_limit"; return false;
    }
    auto file = filesystem_->open(pendingPath(), "w");
    if (!file) { error_ = "local_ota_stage_failed"; return false; }
    const bool written = file.write(reinterpret_cast<const uint8_t *>(bytes.c_str()), bytes.length()) == bytes.length();
    file.close();
    Record verify; String readback;
    if (!written || !read(pendingPath(), verify, &readback) || readback != bytes) {
      error_ = "local_ota_verify_failed"; return false;
    }
    if (!filesystem_->rename(pendingPath(), path())) {
      error_ = "local_ota_commit_failed"; return false;
    }
    record_ = next; known_ = true; error_ = nullptr; ++writes_;
    return true;
  }
  Filesystem *filesystem_ = nullptr;
  Record record_;
  String currentBoot_;
  bool known_ = false, installed_ = false;
  const char *error_ = nullptr;
  uint32_t writes_ = 0;
};
