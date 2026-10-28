#pragma once
// Compile-only transport boundary. This is not a FreeRTOS/hardware simulation.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
uint32_t millis();
struct SshCompileEsp {uint32_t getFreeHeap();};
extern SshCompileEsp ESP;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
