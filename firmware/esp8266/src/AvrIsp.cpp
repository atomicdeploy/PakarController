#include "AvrIsp.h"

namespace {
constexpr uint32_t kSpiClock = 125000;
constexpr uint16_t kPageBytes = 128;
}

bool AvrIsp::fail(const __FlashStringHelper *message) {
  error_ = message;
  leave();
  return false;
}

uint8_t AvrIsp::transfer4(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                          uint8_t *third) {
  SPI.transfer(a);
  SPI.transfer(b);
  const uint8_t thirdValue = SPI.transfer(c);
  if (third != nullptr) *third = thirdValue;
  return SPI.transfer(d);
}

bool AvrIsp::enter() {
  error_ = String();
  pinMode(resetPin_, OUTPUT_OPEN_DRAIN);
  digitalWrite(resetPin_, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(kSpiClock, MSBFIRST, SPI_MODE0));
  digitalWrite(SCK, LOW);
  delay(5);
  digitalWrite(resetPin_, LOW);
  delay(30);
  uint8_t echo = 0;
  transfer4(0xAC, 0x53, 0x00, 0x00, &echo);
  if (echo != 0x53) {
    SPI.endTransaction();
    SPI.end();
    digitalWrite(resetPin_, HIGH);
    pinMode(resetPin_, INPUT);
    error_ = F("AVR did not enter serial programming mode");
    return false;
  }
  active_ = true;
  return true;
}

void AvrIsp::leave() {
  if (active_) SPI.endTransaction();
  SPI.end();
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  digitalWrite(resetPin_, HIGH);
  pinMode(resetPin_, INPUT);
  active_ = false;
  delay(10);
}

bool AvrIsp::readSignature(uint8_t signature[3]) {
  for (uint8_t i = 0; i < 3; ++i) {
    signature[i] = transfer4(0x30, 0x00, i, 0x00);
  }
  return signature[0] == 0x1E && signature[1] == 0x95 && signature[2] == 0x0F;
}

bool AvrIsp::validSignature() {
  uint8_t signature[3];
  if (!readSignature(signature)) return fail(F("Target signature is not ATmega328P"));
  return true;
}

bool AvrIsp::probe(uint8_t signature[3]) {
  if (!enter()) return false;
  const bool valid = readSignature(signature);
  if (!valid) error_ = F("Target signature is not ATmega328P");
  leave();
  return valid;
}

uint8_t AvrIsp::readFlashByte(uint32_t byteAddress) {
  const uint16_t word = static_cast<uint16_t>(byteAddress >> 1);
  return transfer4((byteAddress & 1U) == 0 ? 0x20 : 0x28,
                   static_cast<uint8_t>(word >> 8),
                   static_cast<uint8_t>(word), 0x00);
}

uint8_t AvrIsp::readEepromByte(uint16_t address) {
  return transfer4(0xA0, static_cast<uint8_t>(address >> 8),
                   static_cast<uint8_t>(address), 0x00);
}

uint8_t AvrIsp::readFuse(uint8_t a, uint8_t b) {
  return transfer4(a, b, 0x00, 0x00);
}

bool AvrIsp::backup(fs::FS &filesystem, const char *directory) {
  if (!enter()) return false;
  if (!validSignature()) return false;
  filesystem.mkdir(directory);

  const String flashPath = String(directory) + F("/flash.bin");
  File flash = filesystem.open(flashPath, "w");
  if (!flash) return fail(F("Cannot create AVR flash backup"));
  uint8_t block[128];
  for (uint32_t base = 0; base < kFlashBytes; base += sizeof(block)) {
    for (uint16_t i = 0; i < sizeof(block); ++i) block[i] = readFlashByte(base + i);
    if (flash.write(block, sizeof(block)) != sizeof(block)) {
      flash.close();
      return fail(F("AVR flash backup write failed"));
    }
    yield();
  }
  flash.close();

  const String eepromPath = String(directory) + F("/eeprom.bin");
  File eeprom = filesystem.open(eepromPath, "w");
  if (!eeprom) return fail(F("Cannot create AVR EEPROM backup"));
  for (uint16_t base = 0; base < kEepromBytes; base += sizeof(block)) {
    const uint16_t count = min<uint16_t>(sizeof(block), kEepromBytes - base);
    for (uint16_t i = 0; i < count; ++i) block[i] = readEepromByte(base + i);
    if (eeprom.write(block, count) != count) {
      eeprom.close();
      return fail(F("AVR EEPROM backup write failed"));
    }
    yield();
  }
  eeprom.close();

  const uint8_t lfuse = readFuse(0x50, 0x00);
  const uint8_t hfuse = readFuse(0x58, 0x08);
  const uint8_t efuse = readFuse(0x50, 0x08);
  const uint8_t lock = readFuse(0x58, 0x00);
  File metadata = filesystem.open(String(directory) + F("/metadata.txt"), "w");
  if (!metadata) return fail(F("Cannot create AVR backup metadata"));
  metadata.printf("signature=1E950F\nlfuse=%02X\nhfuse=%02X\nefuse=%02X\nlock=%02X\n",
                  lfuse, hfuse, efuse, lock);
  metadata.close();
  leave();
  return true;
}

bool AvrIsp::programFlash(File &image) {
  if (!image || image.size() != kFlashBytes) {
    error_ = F("AVR image must be exactly 32768 raw bytes");
    return false;
  }
  if (!enter()) return false;
  if (!validSignature()) return false;

  transfer4(0xAC, 0x80, 0x00, 0x00);
  delay(15);
  image.seek(0, SeekSet);
  uint8_t page[kPageBytes];
  for (uint32_t base = 0; base < kFlashBytes; base += kPageBytes) {
    if (image.read(page, sizeof(page)) != sizeof(page)) {
      return fail(F("AVR image ended during page load"));
    }
    for (uint16_t offset = 0; offset < kPageBytes; offset += 2) {
      const uint16_t word = static_cast<uint16_t>((base + offset) >> 1);
      transfer4(0x40, static_cast<uint8_t>(word >> 8),
                static_cast<uint8_t>(word), page[offset]);
      transfer4(0x48, static_cast<uint8_t>(word >> 8),
                static_cast<uint8_t>(word), page[offset + 1]);
    }
    const uint16_t pageWord = static_cast<uint16_t>(base >> 1);
    transfer4(0x4C, static_cast<uint8_t>(pageWord >> 8),
              static_cast<uint8_t>(pageWord), 0x00);
    delay(6);
    yield();
  }

  image.seek(0, SeekSet);
  for (uint32_t base = 0; base < kFlashBytes; base += sizeof(page)) {
    if (image.read(page, sizeof(page)) != sizeof(page)) {
      return fail(F("AVR image ended during verification"));
    }
    for (uint16_t i = 0; i < sizeof(page); ++i) {
      if (readFlashByte(base + i) != page[i]) {
        return fail(F("AVR flash verification mismatch"));
      }
    }
    yield();
  }
  leave();
  return true;
}
