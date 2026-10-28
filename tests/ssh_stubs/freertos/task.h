#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int);
void vTaskDelay(unsigned);
void vTaskDelete(void*);
