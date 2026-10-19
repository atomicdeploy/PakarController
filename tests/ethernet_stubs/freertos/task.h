#pragma once
#include "FreeRTOS.h"
inline TaskHandle_t xTaskGetCurrentTaskHandle(){return ethernetCurrentTask;}
inline int xTaskGetSchedulerState(){return taskSCHEDULER_RUNNING;}
