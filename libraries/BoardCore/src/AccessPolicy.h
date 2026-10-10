#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace BoardCore { namespace Access {
// Adapted from PU850 FirmwareTransfer::secureEquals and Sessions' bounded pool.
inline bool equal(const char *left, const char *right) {
  const size_t a = strlen(left), b = strlen(right);
  size_t difference = a ^ b;
  for (size_t i = 0; i < (a > b ? a : b); ++i)
    difference |= (i < a ? static_cast<uint8_t>(left[i]) : 0) ^
                  (i < b ? static_cast<uint8_t>(right[i]) : 0);
  return difference == 0;
}
inline bool usernameValid(const char *value) {
  const size_t length = strlen(value);
  if (length == 0 || length > 32) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
        !(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.') return false;
  }
  return true;
}
inline bool passwordValid(const char *value) {
  const size_t length = strlen(value);
  if (length < 8 || length > 64) return false;
  for (size_t i = 0; i < length; ++i)
    if (static_cast<uint8_t>(value[i]) < 0x20 || static_cast<uint8_t>(value[i]) > 0x7e) return false;
  return true;
}
inline bool tokenValid(const char *value) {
  if (strlen(value) != 32) return false;
  for (size_t i = 0; i < 32; ++i)
    if (!(value[i] >= '0' && value[i] <= '9') && !(value[i] >= 'a' && value[i] <= 'f')) return false;
  return true;
}
class Sessions {
 public:
  static constexpr uint8_t kCapacity = 8;
  static constexpr uint32_t kIdleMs = 30UL * 60UL * 1000UL;
  bool add(const char *token, uint32_t now) {
    if (!tokenValid(token)) return false;
    for (auto &slot : slots_) {
      if (!slot.active || static_cast<uint32_t>(now - slot.lastActive) >= kIdleMs) {
        memcpy(slot.token, token, 33); slot.active = true; slot.lastActive = now; return true;
      }
    }
    return false;
  }
  bool validate(const char *token, uint32_t now, bool touch = true) {
    if (!tokenValid(token)) return false;
    for (auto &slot : slots_) {
      if (slot.active && static_cast<uint32_t>(now - slot.lastActive) >= kIdleMs) slot.active = false;
      if (slot.active && equal(slot.token, token)) { if (touch) slot.lastActive = now; return true; }
    }
    return false;
  }
  void revoke(const char *token) {
    for (auto &slot : slots_) if (equal(slot.token, token)) slot.active = false;
  }
  void clear() { for (auto &slot : slots_) slot = Slot{}; }
 private:
  struct Slot { char token[33] = {}; uint32_t lastActive = 0; bool active = false; } slots_[kCapacity];
};
} }
