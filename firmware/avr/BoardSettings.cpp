#include "BoardSettings.h"
#include <EEPROM.h>
#include <stddef.h>
namespace {
int address(uint8_t bank) {
  return bank ? EepromLayout::SettingsBankB : EepromLayout::SettingsBankA;
}
}
bool BoardSettingsStore::load(uint8_t bank, Record &record) {
  EEPROM.get(address(bank), record);
  return record.magic == 0x504B && record.commit == 0xA5 &&
         record.crc == BoardCore::WireCodec::crc8(
             reinterpret_cast<const uint8_t *>(&record), offsetof(Record, crc)) &&
         validBoardSettings(record.value);
}
void BoardSettingsStore::begin() {
  values_ = BoardSettings{};
  dirty_ = false;
  persisted_ = false;
  generation_ = 0;
  bank_ = 1;
  Record a, b;
  const bool aValid = load(0, a), bValid = load(1, b);
  if (!aValid && !bValid) return;
  bank_ = bValid && (!aValid || static_cast<int16_t>(b.generation - a.generation) > 0) ? 1 : 0;
  const Record &chosen = bank_ ? b : a;
  values_ = chosen.value;
  generation_ = chosen.generation;
  persisted_ = true;
}
bool BoardSettingsStore::replace(const BoardSettings &value, uint32_t now, bool immediate) {
  if (!validBoardSettings(value)) return false;
  if (memcmp(&values_, &value, sizeof(value)) == 0 && persisted()) return true;
  const BoardSettings previous = values_;
  const bool previousDirty = dirty_;
  const uint32_t previousChangedAt = changedAt_;
  values_ = value;
  changedAt_ = now;
  dirty_ = true;
  if (!immediate || save()) return true;
  values_ = previous;
  dirty_ = previousDirty;
  changedAt_ = previousChangedAt;
  return false;
}
bool BoardSettingsStore::service(uint32_t now, bool allowWrite) {
  return allowWrite && dirty_ && static_cast<uint32_t>(now - changedAt_) >= 1500 && save();
}
bool BoardSettingsStore::save() {
  Record record{0x504B, static_cast<uint16_t>(generation_ + 1), values_, 0, 0};
  record.crc = BoardCore::WireCodec::crc8(
      reinterpret_cast<const uint8_t *>(&record), offsetof(Record, crc));
  const uint8_t destination = bank_ ^ 1U;
  const int base = address(destination);
  EEPROM.update(base + offsetof(Record, commit), 0);
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  for (uint8_t i = 0; i < offsetof(Record, commit); ++i) EEPROM.update(base + i, bytes[i]);
  EEPROM.update(base + offsetof(Record, commit), 0xA5);
  Record verified;
  if (!load(destination, verified) || memcmp(&verified.value, &values_, sizeof(values_)) ||
      verified.generation != record.generation) return false;
  generation_ = record.generation;
  bank_ = destination;
  dirty_ = false;
  persisted_ = true;
  return true;
}
