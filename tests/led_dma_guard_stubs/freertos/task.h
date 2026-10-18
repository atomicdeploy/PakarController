#pragma once
#include "FreeRTOS.h"
BaseType_t xPortInIsrContext();
BaseType_t xPortCanYield();
BaseType_t xTaskGetSchedulerState();
TaskHandle_t xTaskGetCurrentTaskHandle();
