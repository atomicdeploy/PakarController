#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#if defined(ARDUINO_ARCH_ESP32)
#include "../../esp32s3/src/BoardPins.h"
// Must be explicitly enabled only after voltage-safe ISP wiring and reset
// driver polarity have been confirmed. UART operation never needs this gate.
#ifndef PAKAR_S3_ISP_ELECTRICAL_VERIFIED
#define PAKAR_S3_ISP_ELECTRICAL_VERIFIED 0
#endif
#ifndef PAKAR_S3_ISP_RESET_ASSERT_LEVEL
#define PAKAR_S3_ISP_RESET_ASSERT_LEVEL LOW
#endif
static_assert(PAKAR_S3_ISP_ELECTRICAL_VERIFIED == 0 || PAKAR_S3_ISP_ELECTRICAL_VERIFIED == 1,
              "ISP electrical gate must be 0 or 1");
static_assert(PAKAR_S3_ISP_RESET_ASSERT_LEVEL == LOW || PAKAR_S3_ISP_RESET_ASSERT_LEVEL == HIGH,
              "ISP reset driver assertion level must be LOW or HIGH");
#endif

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
#if defined(ARDUINO_ARCH_ESP32)
  SPIClass spi_{FSPI}; // Dedicated SPI2, never W5500's SPI3/HSPI or global SPI.
#else
  SPIClass &spi_ = SPI;
#endif
  bool busStarted_ = false;
  bool active_ = false;
  String error_;
};
