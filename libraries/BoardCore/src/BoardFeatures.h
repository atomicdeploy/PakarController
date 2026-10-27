#pragma once
#include <stdint.h>

// Contract flags are independent of pin aliases. Optional hardware is never
// advertised merely because a symbol was enabled: the adapter must also report
// an installed driver. No I2C access is allowed on the TM1637 A4/A5 pins.
#ifndef PAKAR_PC_MENU
#define PAKAR_PC_MENU 1
#endif
#ifndef PAKAR_PC_MACROS
#define PAKAR_PC_MACROS 1
#endif
#ifndef PAKAR_PC_AUTOMATIONS
#define PAKAR_PC_AUTOMATIONS 1
#endif
#ifndef PAKAR_PC_I2C
#define PAKAR_PC_I2C 0
#endif
#ifndef PAKAR_PC_SENSORS
#define PAKAR_PC_SENSORS 0
#endif
#ifndef PAKAR_PC_PWM
#define PAKAR_PC_PWM 0
#endif
#ifndef PAKAR_PC_RF
#define PAKAR_PC_RF 0
#endif
#ifndef PAKAR_PC_RELAYS
#define PAKAR_PC_RELAYS 0
#endif
#ifndef PAKAR_PC_STATUS_RGB
#define PAKAR_PC_STATUS_RGB 0
#endif
#ifndef PAKAR_PC_AVR_LED
#define PAKAR_PC_AVR_LED 0
#endif
namespace BoardCore {
namespace Features {
enum Feature : uint16_t {
  Menu = 1U << 0, Macros = 1U << 1, Automations = 1U << 2,
  I2c = 1U << 3, Sensors = 1U << 4, Pwm = 1U << 5,
  Rf = 1U << 6, Relays = 1U << 7, StatusRgb = 1U << 8,
  AvrLed = 1U << 9
};
constexpr uint16_t requested =
    (PAKAR_PC_MENU ? static_cast<uint16_t>(Menu) : 0U) | (PAKAR_PC_MACROS ? static_cast<uint16_t>(Macros) : 0U) |
    (PAKAR_PC_AUTOMATIONS ? static_cast<uint16_t>(Automations) : 0U) | (PAKAR_PC_I2C ? static_cast<uint16_t>(I2c) : 0U) |
    (PAKAR_PC_SENSORS ? static_cast<uint16_t>(Sensors) : 0U) | (PAKAR_PC_PWM ? static_cast<uint16_t>(Pwm) : 0U) |
    (PAKAR_PC_RF ? static_cast<uint16_t>(Rf) : 0U) | (PAKAR_PC_RELAYS ? static_cast<uint16_t>(Relays) : 0U) |
    (PAKAR_PC_STATUS_RGB ? static_cast<uint16_t>(StatusRgb) : 0U) | (PAKAR_PC_AVR_LED ? static_cast<uint16_t>(AvrLed) : 0U);
// RF advertises raw receive/transmit only. Learning opcodes remain explicitly
// unavailable until their persistent mapping owner is implemented.
constexpr uint16_t implemented = Menu | Macros | Automations | Rf;
constexpr uint16_t enabled = requested & implemented;
static_assert((PAKAR_PC_MENU == 0 || PAKAR_PC_MENU == 1) &&
              (PAKAR_PC_MACROS == 0 || PAKAR_PC_MACROS == 1) &&
              (PAKAR_PC_AUTOMATIONS == 0 || PAKAR_PC_AUTOMATIONS == 1) &&
              (PAKAR_PC_I2C == 0 || PAKAR_PC_I2C == 1) &&
              (PAKAR_PC_SENSORS == 0 || PAKAR_PC_SENSORS == 1) &&
              (PAKAR_PC_PWM == 0 || PAKAR_PC_PWM == 1) &&
              (PAKAR_PC_RF == 0 || PAKAR_PC_RF == 1) &&
              (PAKAR_PC_RELAYS == 0 || PAKAR_PC_RELAYS == 1) &&
              (PAKAR_PC_STATUS_RGB == 0 || PAKAR_PC_STATUS_RGB == 1) &&
              (PAKAR_PC_AVR_LED == 0 || PAKAR_PC_AVR_LED == 1),
              "PAKAR_PC feature flags must be 0 or 1");
// Fail closed rather than let a later build claim working absent drivers.
static_assert((requested & ~implemented) == 0,
              "Optional PCController hardware needs its adapter and pin profile before enabling");
}
}
