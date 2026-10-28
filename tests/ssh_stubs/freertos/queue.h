#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned,unsigned);
BaseType_t xQueueReset(QueueHandle_t);
BaseType_t xQueueReceive(QueueHandle_t,void*,unsigned);
BaseType_t xQueueSend(QueueHandle_t,const void*,unsigned);
unsigned uxQueueSpacesAvailable(QueueHandle_t);
unsigned uxQueueMessagesWaiting(QueueHandle_t);
