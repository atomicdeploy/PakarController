#include "AccessConfig.h"
#include <stddef.h>

AccessConfig accessConfig;
namespace { constexpr char kPath[] = "/access.bin"; constexpr char kPending[] = "/access.pending"; }
uint32_t AccessConfig::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < offsetof(Record, checksum); ++i) value = (value ^ bytes[i]) * 16777619UL;
  return value;
}
bool AccessConfig::begin(fs::FS &filesystem, bool mounted) {
  if (!mounted) { ready_ = false; filesystem_ = nullptr; error_ = F("Access filesystem is unavailable; physical recovery is required"); return false; }
  filesystem_ = &filesystem;
  record_.magic = 0x504b4155UL;
  record_.schema = 1;
  strcpy(record_.username, "admin"); strcpy(record_.password, "pakarcontroller");
  record_.checksum = checksum(record_);
  ready_ = false;
  if (!filesystem.exists(kPath)) { ready_ = true; return true; }
  File file = filesystem.open(kPath, "r");
  Record stored{};
  const bool complete = file && file.size() == sizeof(stored) &&
      file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) == sizeof(stored);
  if (file) file.close();
  if (!complete || stored.magic != record_.magic || stored.schema != 1 ||
      stored.username[32] != 0 || stored.password[64] != 0 ||
      !BoardCore::Access::usernameValid(stored.username) ||
      !BoardCore::Access::passwordValid(stored.password) || stored.checksum != checksum(stored)) {
    error_ = F("Access settings are damaged; recovery requires the physical USB path");
    return false; // Never silently restore a public password after corruption.
  }
  record_ = stored; ready_ = true; return true;
}
bool AccessConfig::change(const String &currentPassword, const String &username, const String &password) {
  if (!ready_ || !BoardCore::Access::equal(currentPassword.c_str(), record_.password)) {
    error_ = F("Current password is incorrect"); return false;
  }
  return provisionFromPhysicalUsb(username, password);
}
bool AccessConfig::provisionFromPhysicalUsb(const String &username, const String &password) {
  if (!filesystem_) { error_ = F("Access storage unavailable"); return false; }
  if (!BoardCore::Access::usernameValid(username.c_str()) || !BoardCore::Access::passwordValid(password.c_str())) {
    error_ = F("Username must be 1..32 letters/digits/._-; password must be 8..64 printable ASCII characters"); return false;
  }
  Record next = record_;
  memset(next.username, 0, sizeof(next.username)); memset(next.password, 0, sizeof(next.password));
  username.toCharArray(next.username, sizeof(next.username)); password.toCharArray(next.password, sizeof(next.password));
  next.checksum = checksum(next);
  File file = filesystem_->open(kPending, "w");
  if (!file) { error_ = F("Cannot stage access settings"); return false; }
  const bool written = file.write(reinterpret_cast<const uint8_t *>(&next), sizeof(next)) == sizeof(next);
  file.close();
  File verify = filesystem_->open(kPending, "r"); Record readback{};
  const bool valid = verify && verify.size() == sizeof(next) &&
      verify.read(reinterpret_cast<uint8_t *>(&readback), sizeof(readback)) == sizeof(readback) &&
      memcmp(&next, &readback, sizeof(next)) == 0;
  if (verify) verify.close();
  if (!written || !valid || !filesystem_->rename(kPending, kPath)) {
    error_ = F("Cannot commit access settings; previous credentials remain active"); return false;
  }
  record_ = next; ready_ = true; error_ = String(); return true;
}
