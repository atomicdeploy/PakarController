#pragma once
#include "FreeRTOS.h"
#include <condition_variable>
#include <mutex>
#include <vector>
struct StaticQueue_t{
 std::mutex mutex;std::condition_variable changed;std::vector<uint8_t> bytes;bool full=false;
};
using QueueHandle_t=StaticQueue_t*;
QueueHandle_t xQueueCreateStatic(unsigned,size_t,uint8_t*,StaticQueue_t*);
int xQueueOverwrite(QueueHandle_t,const void*);
int xQueueReceive(QueueHandle_t,void*,TickType_t);
unsigned uxQueueMessagesWaiting(QueueHandle_t);
