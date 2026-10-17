#pragma once
#include "FreeRTOS.h"
int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int);
void vTaskDelay(TickType_t);
void vTaskDelete(void*);
