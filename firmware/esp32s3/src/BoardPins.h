#pragma once
#include <stdint.h>
#if defined(ARDUINO_ARCH_ESP32) && __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#include <GameRole.h>

// Normally-open game reset button to GND; not EN or AVR ISP RESET.
// Disabled on table builds. -1 disables it on a custom keycontroller build.
#ifndef PAKAR_S3_GAME_RESET_PIN
#if PAKAR_FIRST_WINS_ENABLED
#define PAKAR_S3_GAME_RESET_PIN 21
#else
#define PAKAR_S3_GAME_RESET_PIN -1
#endif
#endif
static_assert(PAKAR_S3_GAME_RESET_PIN == -1 || PAKAR_S3_GAME_RESET_PIN == 21,
              "Game reset pin requires a collision-free reviewed pin mapping");

// PROVISIONAL common DevKitC-1 plan, not confirmation of the user's module.
// GPIO numbers, never NodeMCU Dx names. Confirm board schematic before wiring.
// UART1 conventional routing: S3 TX17 -> AVR RXD; AVR TXD -> S3 RX18.
// Overrides must match the actual wires; older provisional plans reversed this pair.
#ifndef PAKAR_S3_AVR_RX
#define PAKAR_S3_AVR_RX 18
#endif
#ifndef PAKAR_S3_AVR_TX
#define PAKAR_S3_AVR_TX 17
#endif
#ifndef PAKAR_S3_LED_DATA
#define PAKAR_S3_LED_DATA 4
#endif
#ifndef PAKAR_S3_ISP_RESET
#define PAKAR_S3_ISP_RESET 10
#endif
#ifndef PAKAR_S3_ISP_SCK
#define PAKAR_S3_ISP_SCK 12
#endif
#ifndef PAKAR_S3_ISP_MISO
#define PAKAR_S3_ISP_MISO 13
#endif
#ifndef PAKAR_S3_ISP_MOSI
#define PAKAR_S3_ISP_MOSI 11
#endif

// W5500 mapping used by the optional -S3Ethernet native driver. Keep it on
// independent SPI3/HSPI; AVR ISP owns SPI2/FSPI and has no ordinary CS input.
#ifndef PAKAR_S3_W5500_SCK
#define PAKAR_S3_W5500_SCK 5
#endif
#ifndef PAKAR_S3_W5500_MISO
#define PAKAR_S3_W5500_MISO 6
#endif
#ifndef PAKAR_S3_W5500_MOSI
#define PAKAR_S3_W5500_MOSI 7
#endif
#ifndef PAKAR_S3_W5500_CS
#define PAKAR_S3_W5500_CS 8
#endif
#ifndef PAKAR_S3_W5500_RESET
#define PAKAR_S3_W5500_RESET 9
#endif
#ifndef PAKAR_S3_W5500_IRQ
#define PAKAR_S3_W5500_IRQ 14
#endif

// Optional LCD backpack: never share the AVR's A4/A5 TM1637 connector.
#ifndef PAKAR_S3_LCD_SDA
#define PAKAR_S3_LCD_SDA 15
#endif
#ifndef PAKAR_S3_LCD_SCL
#define PAKAR_S3_LCD_SCL 16
#endif

namespace PakarS3 {
constexpr uint8_t kPins[] = {PAKAR_S3_AVR_RX, PAKAR_S3_AVR_TX,
    PAKAR_S3_LED_DATA, PAKAR_S3_ISP_RESET, PAKAR_S3_ISP_SCK,
    PAKAR_S3_ISP_MISO, PAKAR_S3_ISP_MOSI,
    PAKAR_S3_W5500_SCK, PAKAR_S3_W5500_MISO, PAKAR_S3_W5500_MOSI,
    PAKAR_S3_W5500_CS, PAKAR_S3_W5500_RESET, PAKAR_S3_W5500_IRQ,
    PAKAR_S3_LCD_SDA, PAKAR_S3_LCD_SCL};
constexpr bool safePins() {
  for (unsigned i = 0; i < sizeof(kPins); ++i) {
    // Deliberately conservative: avoid boot straps, USB, module memory,
    // bridge UART, built-in RGB and model-dependent high-numbered pins.
    // DevKitC-1 J3-17 GPIO47 is an explicit alternate LED output only.
    // It is separate from GPIO48 RGB, USB19/20 and OPI memory35..37.
    // GPIO48 diagnostic replaces the onboard status owner, never shares it.
    const bool alternateLed = i == 2 && (kPins[i] == 47 || kPins[i] == 48);
    if (!alternateLed && (kPins[i] < 4 || kPins[i] > 18)) return false;
    for (unsigned j = 0; j < i; ++j) if (kPins[i] == kPins[j]) return false;
  }
  return true;
}
static_assert(safePins(), "S3 pin plan overlaps or needs explicit schematic review");
constexpr uint32_t kAvrBaud = 38400; // Preserve AVR compatibility initially.
constexpr unsigned kAvrRxBuffer = 2048;
constexpr unsigned kAvrTxBuffer = 256;
} // namespace PakarS3
