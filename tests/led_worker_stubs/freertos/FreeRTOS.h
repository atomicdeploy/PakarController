#pragma once
#include <cstdint>
using TickType_t=uint32_t;
constexpr int pdTRUE=1,pdPASS=1;
#define pdMS_TO_TICKS(ms) (ms)
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
void fixtureCriticalEnter();
void fixtureCriticalExit();
#define portENTER_CRITICAL(mux) ((void)(mux),fixtureCriticalEnter())
#define portEXIT_CRITICAL(mux) ((void)(mux),fixtureCriticalExit())
