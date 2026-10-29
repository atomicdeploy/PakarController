#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "PeerFence.h"

namespace BoardCore { namespace CloudCommand {
enum class Result : uint8_t {
  Allowed, InvalidIdentity, MissingEnvelope, WrongPeer, InvalidRequestId,
  ClockUnavailable, ClockRegressed, Expired, DeadlineTooFar, Duplicate, Capacity
};
inline const char *code(Result value) {
  switch (value) {
    case Result::Allowed: return "allowed";
    case Result::InvalidIdentity: return "invalid_identity";
    case Result::MissingEnvelope: return "missing_envelope";
    case Result::WrongPeer: return "wrong_peer";
    case Result::InvalidRequestId: return "invalid_request_id";
    case Result::ClockUnavailable: return "clock_unavailable";
    case Result::ClockRegressed: return "clock_regressed";
    case Result::Expired: return "expired";
    case Result::DeadlineTooFar: return "deadline_too_far";
    case Result::Duplicate: return "duplicate";
    case Result::Capacity: return "capacity";
  }
  return "invalid_admission_result";
}

// Adapt the existing mobile BridgeAdmission's consume-before-dispatch,
// identity/deadline checks and finite budget to a heap-free MCU policy. PeerFence
// supplies the actual shared board/boot validators. Identity is correlation,
// not authentication; the caller must authenticate transport and serialize calls.
class Guard {
 public:
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  static constexpr size_t kCapacity = 128;
#else
  static constexpr size_t kCapacity = 32;
#endif
  static constexpr uint32_t kMinimumEpoch = 1700000000U;
  static constexpr uint32_t kMaximumLifetime = 60U;

  bool setIdentity(const char *board, const char *boot) {
    if (!PeerFence::ready(board, boot)) {
      ready_ = false;
      // Retain reservations/high-water if the same identity is later restored.
      // A temporary invalid identity must not become a replay-cache reset.
      return false;
    }
    if (strcmp(board_, board) != 0 || strcmp(boot_, boot) != 0) {
      for (auto &entry : entries_) entry = Entry{};
      memcpy(board_, board, strlen(board) + 1);
      memcpy(boot_, boot, 33);
      // Wall-clock regression remains unsafe across identity reconfiguration.
    }
    ready_ = true;
    return true;
  }

  static bool readOnly(const char *command) {
    return command && (!strcmp(command, "status") ||
        !strcmp(command, "telemetry") || !strcmp(command, "firmware-status") ||
        !strcmp(command, "peer-network-status") || !strcmp(command, "peer-relay-result") ||
        !strcmp(command, "peer-directory"));
  }

  Result admit(const char *command, const char *board, const char *boot,
      const char *requestId, uint32_t expiresAt, uint32_t nowEpoch,
      uint32_t nowMillis) {
    // Deadlines/cache expiry live entirely in UTC seconds. millis() rollover
    // neither clears reservations nor extends deadlines; no per-entry tick cost.
    (void)nowMillis;
    const bool regressed = nowEpoch >= kMinimumEpoch && nowEpoch < highWater_;
    if (nowEpoch >= kMinimumEpoch && nowEpoch > highWater_) highWater_ = nowEpoch;
    if (readOnly(command)) return Result::Allowed;
    if (!ready_) return Result::InvalidIdentity;
    if (!board || !boot || !requestId || !expiresAt) return Result::MissingEnvelope;
    if (!PeerFence::ready(board, boot) || strcmp(board, board_) || strcmp(boot, boot_))
      return Result::WrongPeer;
    if (!requestValid(requestId)) return Result::InvalidRequestId;
    if (nowEpoch < kMinimumEpoch) return Result::ClockUnavailable;
    if (regressed) return Result::ClockRegressed;
    if (expiresAt <= nowEpoch) return Result::Expired;
    if (expiresAt - nowEpoch > kMaximumLifetime) return Result::DeadlineTooFar;
    Entry *free = nullptr;
    for (auto &entry : entries_) {
      if (entry.id[0] && entry.expiresAt > nowEpoch) {
        if (!strcmp(entry.id, requestId)) return Result::Duplicate;
      } else if (!free) free = &entry;
    }
    if (!free) return Result::Capacity; // No eviction of an unexpired ID.
    memcpy(free->id, requestId, strlen(requestId) + 1);
    free->expiresAt = expiresAt; // Consumed before the caller invokes any handler.
    return Result::Allowed;
  }

 private:
  static bool requestValid(const char *id) {
    size_t n = 0;
    for (; n <= 40 && id[n]; ++n) {
      const char c = id[n];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return n > 0 && n <= 40;
  }
  struct Entry { char id[41]{}; uint32_t expiresAt = 0; };
  static_assert(sizeof(Entry) <= 48, "Cloud replay-entry memory budget changed");
  Entry entries_[kCapacity]{};
  char board_[65]{};
  char boot_[33]{};
  uint32_t highWater_ = 0;
  bool ready_ = false;
};
static_assert(sizeof(Guard) <= Guard::kCapacity * 48 + 112,
              "Cloud command admission must remain fixed/bounded");
} }
