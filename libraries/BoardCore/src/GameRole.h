#pragma once
#include <stdint.h>
#if defined(ARDUINO_ARCH_ESP32) && __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
// A single business program; effects/diagnostics remain presentation services.
#ifndef PAKAR_GAME_MODE
#define PAKAR_GAME_MODE 1
#endif
#if PAKAR_GAME_MODE != 1 && PAKAR_GAME_MODE != 2
#error "PAKAR_GAME_MODE must select table-selection (1) or first-wins (2)"
#endif
#define PAKAR_TABLE_SELECTION (PAKAR_GAME_MODE == 1)
#define PAKAR_FIRST_WINS_ENABLED (PAKAR_GAME_MODE == 2)
#define PAKAR_SNAKE_ENABLED 0
namespace BoardCore { namespace GameRole {
constexpr uint8_t mode = PAKAR_GAME_MODE;
constexpr const char *name = PAKAR_TABLE_SELECTION ? "table-selection" : "first-wins";
constexpr bool accepts(uint8_t value) { return value == mode; }
} }
