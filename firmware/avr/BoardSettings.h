#pragma once
#include <Arduino.h>
#include <string.h>
#include <WireCodec.h>
#include <pccontroller/EepromLayout.h>

// Schema 0x50 is an explicit Pakar profile, not fake PCController voltage/PWM/
// motion fields. SETTINGS requests/ACKs/opcodes retain the donor contract.
struct __attribute__((packed)) BoardSettings {
  static constexpr uint8_t Schema = 0x50;
  uint8_t schema = Schema;
  uint8_t flags = 2; // silent, welcome, save-last-page; never the Prog latch.
  uint8_t displayBrightness = 5;
  uint16_t streamPeriodMs = 0; // changed-only by default; polling still works.
  uint16_t ninthThreshold = 512;
  uint8_t defaultPage = 0;
  uint8_t visiblePages = 0x3F;
  uint8_t packedOrder[3] = {0x10, 0x32, 0x54};
  uint8_t nameLength = 0;
  uint8_t name[8] = {};
};
static_assert(sizeof(BoardSettings) == 21, "Pakar settings wire layout changed");

inline uint8_t firstVisiblePage(uint8_t mask, const uint8_t *order) {
  if (!mask || (mask & ~0x3FU)) return 0xFF;
  uint8_t seen = 0, first = 0xFF;
  for (uint8_t rank = 0; rank < 6; ++rank) {
    const uint8_t page = (order[rank >> 1] >> ((rank & 1) * 4)) & 0x0F;
    if (page >= 6 || (seen & (1U << page))) return 0xFF;
    seen |= 1U << page;
    if (first == 0xFF && (mask & (1U << page))) first = page;
  }
  return seen == 0x3F ? first : 0xFF;
}

inline bool validBoardSettings(const BoardSettings &value) {
  if (value.schema != BoardSettings::Schema || (value.flags & ~7U) ||
      value.displayBrightness > 7 ||
      (value.streamPeriodMs && value.streamPeriodMs < 100) ||
      value.ninthThreshold < 1 || value.ninthThreshold > 1023 ||
      value.defaultPage >= 6 || !(value.visiblePages & (1U << value.defaultPage)) ||
      firstVisiblePage(value.visiblePages, value.packedOrder) == 0xFF ||
      value.nameLength > sizeof(value.name)) return false;
  for (uint8_t i = 0; i < sizeof(value.name); ++i)
    if (i < value.nameLength ? value.name[i] < 0x20 || value.name[i] > 0x7E
                            : value.name[i] != 0) return false;
  return true;
}

// Dual-bank commit-last/CRC/readback, EEPROM.update wear reduction, and the
// donor's 1500ms coalescing. Invalid EEPROM never silently persists defaults.
class BoardSettingsStore {
 public:
  void begin();
  bool replace(const BoardSettings &value, uint32_t now, bool immediate = true);
  bool service(uint32_t now, bool allowWrite = true);
  const BoardSettings &values() const { return values_; }
  bool persisted() const { return persisted_ && !dirty_; }
 private:
  struct __attribute__((packed)) Record {
    uint16_t magic;
    uint16_t generation;
    BoardSettings value;
    uint8_t crc;
    uint8_t commit;
  };
  static_assert(sizeof(Record) <= EepromLayout::SettingsBankBytes, "Settings bank overflow");
  bool load(uint8_t bank, Record &record);
  bool save();
  BoardSettings values_{};
  uint16_t generation_ = 0;
  uint8_t bank_ = 1;
  uint32_t changedAt_ = 0;
  bool persisted_ = false;
  bool dirty_ = false;
};
