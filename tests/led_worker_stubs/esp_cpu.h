#pragma once
#include <stdint.h>
namespace Fixture {extern uint32_t cycle;}
inline uint32_t esp_cpu_get_cycle_count(){return ++Fixture::cycle;}
