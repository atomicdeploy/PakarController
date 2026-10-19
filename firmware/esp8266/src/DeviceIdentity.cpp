#include "DeviceIdentity.h"
#include "ConfigurationChanges.h"
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
  memset(&record_, 0, sizeof(record_)); record_.magic = 0x504b4944UL; record_.schema = 2;
  char initial[64];
  if (!BoardCore::Identity::normalizeName(PAKAR_DEFAULT_HOSTNAME, initial)) {
    error_ = F("Invalid build default hostname"); return false;
  }
  memcpy(record_.hostname, initial, sizeof(initial)); memcpy(record_.displayName, initial, sizeof(initial)); record_.checksum = checksum(record_);
  if (!mounted) { error_ = F("Identity storage unavailable"); return false; }
  // Persist the initial label so later generic OTA builds cannot undo it.
  if (!filesystem.exists(kPath)) return rename(record_.hostname);
  File file = filesystem.open(kPath, "r"); Record stored{}; char normalized[64];
  struct Legacy { uint32_t magic; uint8_t schema; char hostname[64]; uint32_t checksum; } __attribute__((packed));
  if (file && file.size() == sizeof(Legacy)) {
    Legacy legacy{}; const bool read = file.read(reinterpret_cast<uint8_t *>(&legacy), sizeof(legacy)) == sizeof(legacy); file.close();
    uint32_t digest = 2166136261UL;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&legacy);
    for (size_t i = 0; i < offsetof(Legacy, checksum); ++i) digest = (digest ^ bytes[i]) * 16777619UL;
    if (read && legacy.magic == record_.magic && legacy.schema == 1 && legacy.hostname[63] == 0 &&
        digest == legacy.checksum && BoardCore::Identity::normalizeName(legacy.hostname, normalized) && strcmp(legacy.hostname, normalized) == 0) {
      // Keep the deployed DNS name active even if storage cannot migrate now.
      memcpy(record_.hostname, legacy.hostname, sizeof(legacy.hostname));
      memcpy(record_.displayName, legacy.hostname, sizeof(legacy.hostname));
      record_.checksum = checksum(record_);
      return configure(legacy.hostname, legacy.hostname); // One verified atomic migration; preserve DNS identity.
    }
    error_ = F("Legacy identity record damaged; use authenticated identity recovery"); return false;
  }
  const bool valid = file && file.size() == sizeof(stored) &&
      file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) == sizeof(stored) &&
      stored.magic == record_.magic && stored.schema == 2 && stored.hostname[63] == 0 &&
      stored.checksum == checksum(stored) &&
      stored.displayName[63] == 0 && BoardCore::Identity::validDisplayName(stored.displayName) &&
      BoardCore::Identity::normalizeName(stored.hostname, normalized) &&
      strcmp(stored.hostname, normalized) == 0;
  if (file) file.close();
  if (!valid) { error_ = F("Identity record damaged; use authenticated rename or physical USB recovery"); return false; }
  record_ = stored; ready_ = true; return true;
}
bool DeviceIdentity::rename(const String &name) {
  return configure(name, record_.displayName);
}
bool DeviceIdentity::configure(const String &name, const String &displayName) {
  char normalized[64];
  if (name.length() != strlen(name.c_str()) || !BoardCore::Identity::normalizeName(name.c_str(), normalized)) {
    error_ = F("Use 1..63 letters/digits/hyphens, no leading/trailing hyphen; optional .local suffix"); return false;
  }
  if (displayName.length() != strlen(displayName.c_str()) || !BoardCore::Identity::validDisplayName(displayName.c_str())) {
    error_ = F("Board name must be 1..63 UTF-8 bytes without controls or edge spaces"); return false;
  }
  if (!filesystem_) { error_ = F("Identity storage unavailable"); return false; }
  if (ready_ && strcmp(record_.hostname, normalized) == 0 && displayName == record_.displayName) { error_ = String(); return true; }
  Record next = record_; memcpy(next.hostname, normalized, sizeof(normalized));
  memset(next.displayName, 0, sizeof(next.displayName)); memcpy(next.displayName, displayName.c_str(), displayName.length()); next.checksum = checksum(next);
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
  record_ = next; ready_ = true; error_ = String();
  PakarConfiguration::changes().saved(PakarConfiguration::Identity);return true;
}
