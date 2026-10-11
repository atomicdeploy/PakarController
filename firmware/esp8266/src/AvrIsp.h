#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>

class AvrIsp {
 public:
  static constexpr uint32_t kFlashBytes = 32768;
  static constexpr uint16_t kEepromBytes = 1024;

  explicit AvrIsp(uint8_t resetPin) : resetPin_(resetPin) {}

  bool probe(uint8_t signature[3]);
  bool backup(fs::FS &filesystem, const char *directory);
  // Recovery EEPROM must be a durable snapshot taken AFTER Prog is armed.
  // Refuse erase unless every byte matches the still-running target.
  bool programFlash(File &image, File &recoveryEeprom);
  const String &lastError() const { return error_; }

 private:
  bool enter();
  void leave();
  uint8_t transfer4(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                    uint8_t *third = nullptr);
  uint8_t readFlashByte(uint32_t byteAddress);
  uint8_t readEepromByte(uint16_t address);
  uint8_t readFuse(uint8_t a, uint8_t b);
  bool readSignature(uint8_t signature[3]);
  bool fail(const __FlashStringHelper *message);
  bool validSignature();

  uint8_t resetPin_;
  bool active_ = false;
  String error_;
};
