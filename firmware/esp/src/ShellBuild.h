#pragma once
#if defined(ARDUINO_ARCH_ESP32) && __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#ifndef PAKAR_TELNET_SESSIONS
#define PAKAR_TELNET_SESSIONS 2
#endif
#ifndef PAKAR_SERIAL_SHELL_ENABLED
#define PAKAR_SERIAL_SHELL_ENABLED 1
#endif
#ifndef PAKAR_TELNET_ENABLED
#define PAKAR_TELNET_ENABLED 1
#endif
