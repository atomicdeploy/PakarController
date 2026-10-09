#include "ProgrammingLatch.h"

#include <EEPROM.h>

namespace {
constexpr int kAddress = 1008;
constexpr uint8_t kMagic0 = 0x50;  // P
constexpr uint8_t kMagic1 = 0x4B;  // K
constexpr uint8_t kSchema = 1;

struct Record {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t schema;
  uint8_t flags;
  uint8_t check;
};

uint8_t checksum(const Record &record) {
  return static_cast<uint8_t>(record.magic0 ^ record.magic1 ^ record.schema ^
                              record.flags ^ 0xA5U);
}
}  // namespace

ProgrammingLatch programmingLatch;

void ProgrammingLatch::begin() {
  Record record{};
  EEPROM.get(kAddress, record);
  active_ = record.magic0 == kMagic0 && record.magic1 == kMagic1 &&
            record.schema == kSchema && record.check == checksum(record) &&
            (record.flags & 0x01U) != 0;
}

bool ProgrammingLatch::set(bool active) {
  const Record record = {kMagic0, kMagic1, kSchema,
                         static_cast<uint8_t>(active ? 1U : 0U), 0};
  Record stored = record;
  stored.check = checksum(stored);
  EEPROM.put(kAddress, stored);

  Record verified{};
  EEPROM.get(kAddress, verified);
  const bool valid = verified.magic0 == kMagic0 && verified.magic1 == kMagic1 &&
                     verified.schema == kSchema &&
                     verified.check == checksum(verified) &&
                     ((verified.flags & 0x01U) != 0) == active;
  if (valid) active_ = active;
  return valid;
}
