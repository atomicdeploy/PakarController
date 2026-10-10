#include "DeviceIdentity.h"
#include <stddef.h>
DeviceIdentity deviceIdentity;
namespace { constexpr char kPath[] = "/identity.bin"; constexpr char kPending[] = "/identity.pending"; }
uint32_t DeviceIdentity::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < offsetof(Record, checksum); ++i) value = (value ^ bytes[i]) * 16777619UL;
  return value;
}
bool DeviceIdentity::begin(fs::FS &filesystem, bool mounted) {
  ready_ = false; error_ = String(); filesystem_ = mounted ? &filesystem : nullptr;
  memset(&record_, 0, sizeof(record_)); record_.magic = 0x504b4944UL; record_.schema = 1;
  char initial[64];
  if (!BoardCore::Identity::normalizeName(PAKAR_DEFAULT_HOSTNAME, initial)) {
    error_ = F("Invalid build default hostname"); return false;
  }
  memcpy(record_.hostname, initial, sizeof(initial)); record_.checksum = checksum(record_);
  if (!mounted) { error_ = F("Identity storage unavailable"); return false; }
  // Persist the initial label so later generic OTA builds cannot undo it.
  if (!filesystem.exists(kPath)) return rename(record_.hostname);
  File file = filesystem.open(kPath, "r"); Record stored{}; char normalized[64];
  const bool valid = file && file.size() == sizeof(stored) &&
      file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) == sizeof(stored) &&
      stored.magic == record_.magic && stored.schema == 1 && stored.hostname[63] == 0 &&
      stored.checksum == checksum(stored) &&
      BoardCore::Identity::normalizeName(stored.hostname, normalized) &&
      strcmp(stored.hostname, normalized) == 0;
  if (file) file.close();
  if (!valid) { error_ = F("Identity record damaged; use authenticated rename or physical USB recovery"); return false; }
  record_ = stored; ready_ = true; return true;
}
bool DeviceIdentity::rename(const String &name) {
  char normalized[64];
  if (name.length() != strlen(name.c_str()) || !BoardCore::Identity::normalizeName(name.c_str(), normalized)) {
    error_ = F("Use 1..63 letters/digits/hyphens, no leading/trailing hyphen; optional .local suffix"); return false;
  }
  if (!filesystem_) { error_ = F("Identity storage unavailable"); return false; }
  if (ready_ && strcmp(record_.hostname, normalized) == 0) { error_ = String(); return true; }
  Record next = record_; memcpy(next.hostname, normalized, sizeof(normalized)); next.checksum = checksum(next);
  File file = filesystem_->open(kPending, "w");
  if (!file) { error_ = F("Cannot stage device name"); return false; }
  const bool written = file.write(reinterpret_cast<const uint8_t *>(&next), sizeof(next)) == sizeof(next);
  file.close();
  File verify = filesystem_->open(kPending, "r"); Record readback{};
  const bool valid = verify && verify.size() == sizeof(next) &&
      verify.read(reinterpret_cast<uint8_t *>(&readback), sizeof(readback)) == sizeof(readback) &&
      memcmp(&readback, &next, sizeof(next)) == 0;
  if (verify) verify.close();
  if (!written || !valid || !filesystem_->rename(kPending, kPath)) {
    error_ = F("Cannot commit device name; previous name remains active"); return false;
  }
  record_ = next; ready_ = true; error_ = String(); return true;
}
