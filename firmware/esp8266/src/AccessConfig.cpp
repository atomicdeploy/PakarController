#include "AccessConfig.h"
#include <stddef.h>

AccessConfig accessConfig;
namespace {
constexpr char kPath[] = "/access.bin"; constexpr char kPending[] = "/access.pending";
struct LegacyRecord {
  uint32_t magic; uint8_t schema; char username[33]; char password[65]; uint32_t checksum;
} __attribute__((packed));
uint32_t digest(const void *record, size_t count) {
  const auto *bytes = static_cast<const uint8_t *>(record);
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < count; ++i) value = (value ^ bytes[i]) * 16777619UL;
  return value;
}
}
uint32_t AccessConfig::checksum(const Record &record) {
  return digest(&record, offsetof(Record, checksum));
}
bool AccessConfig::begin(fs::FS &filesystem, bool mounted) {
  if (!mounted) { ready_ = false; filesystem_ = nullptr; error_ = F("Access filesystem is unavailable; physical recovery is required"); return false; }
  filesystem_ = &filesystem;
  record_ = Record{}; record_.magic = 0x504b4155UL;
  record_.schema = 2;
  strcpy(record_.username, "admin"); strcpy(record_.password, "pakarcontroller");
  record_.checksum = checksum(record_);
  ready_ = false;
  if (!filesystem.exists(kPath)) { ready_ = true; return true; }
  File file = filesystem.open(kPath, "r");
  Record stored{};
  if (file && file.size() == sizeof(LegacyRecord)) {
    LegacyRecord legacy{};
    const bool valid = file.read(reinterpret_cast<uint8_t *>(&legacy), sizeof(legacy)) == sizeof(legacy) &&
        legacy.magic == record_.magic && legacy.schema == 1 && legacy.username[32] == 0 &&
        legacy.password[64] == 0 && BoardCore::Access::usernameValid(legacy.username) &&
        BoardCore::Access::passwordValid(legacy.password) &&
        legacy.checksum == digest(&legacy, offsetof(LegacyRecord, checksum));
    file.close();
    if (!valid) { error_ = F("Access settings are damaged; physical USB recovery required"); return false; }
    memcpy(stored.username, legacy.username, sizeof(stored.username));
    memcpy(stored.password, legacy.password, sizeof(stored.password));
    stored.magic = record_.magic; stored.schema = 2;
    // Old factory credentials were mandatory, not an explicit opt-in. Migrate
    // those to disabled; preserve protection for an explicitly configured user.
    stored.enabled = !(BoardCore::Access::equal(legacy.username, "admin") &&
        BoardCore::Access::equal(legacy.password, "pakarcontroller"));
    stored.checksum = checksum(stored);
    return save(stored);
  }
  const bool complete = file && file.size() == sizeof(stored) &&
      file.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) == sizeof(stored);
  if (file) file.close();
  if (!complete || stored.magic != record_.magic || stored.schema != 2 || stored.enabled > 1 ||
      stored.username[32] != 0 || stored.password[64] != 0 ||
      !BoardCore::Access::usernameValid(stored.username) ||
      !BoardCore::Access::passwordValid(stored.password) || stored.checksum != checksum(stored)) {
    error_ = F("Access settings are damaged; recovery requires the physical USB path");
    return false; // Never silently restore a public password after corruption.
  }
  record_ = stored; ready_ = true; return true;
}
bool AccessConfig::change(const String &currentPassword, const String &username, const String &password) {
  return configure(currentPassword, true, username, password);
}
bool AccessConfig::configure(const String &currentPassword, bool enabled,
                             const String &username, const String &password) {
  if (!ready_ || (this->enabled() && !BoardCore::Access::equal(currentPassword.c_str(), record_.password))) {
    error_ = F("Current password is incorrect"); return false;
  }
  Record next = record_;
  if (username.length() || password.length() || (enabled && !this->enabled())) {
    if (!BoardCore::Access::usernameValid(username.c_str()) || !BoardCore::Access::passwordValid(password.c_str())) {
      error_ = F("Set a valid username and an 8..64 character password to enable or change authentication"); return false;
    }
    memset(next.username, 0, sizeof(next.username)); memset(next.password, 0, sizeof(next.password));
    username.toCharArray(next.username, sizeof(next.username)); password.toCharArray(next.password, sizeof(next.password));
  }
  next.enabled = enabled ? 1 : 0; next.checksum = checksum(next);
  return save(next);
}
bool AccessConfig::provisionFromPhysicalUsb(const String &username, const String &password) {
  if (!filesystem_) { error_ = F("Access storage unavailable"); return false; }
  if (!BoardCore::Access::usernameValid(username.c_str()) || !BoardCore::Access::passwordValid(password.c_str())) {
    error_ = F("Username must be 1..32 letters/digits/._-; password must be 8..64 printable ASCII characters"); return false;
  }
  Record next = record_;
  memset(next.username, 0, sizeof(next.username)); memset(next.password, 0, sizeof(next.password));
  username.toCharArray(next.username, sizeof(next.username)); password.toCharArray(next.password, sizeof(next.password));
  next.magic = 0x504b4155UL; next.schema = 2; next.enabled = 1;
  next.checksum = checksum(next);
  return save(next);
}
bool AccessConfig::save(const Record &next) {
  if (!filesystem_) { error_ = F("Access storage unavailable"); return false; }
  if (ready_ && filesystem_->exists(kPath) && memcmp(&next, &record_, sizeof(next)) == 0) {
    error_ = String(); return true;
  }
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
