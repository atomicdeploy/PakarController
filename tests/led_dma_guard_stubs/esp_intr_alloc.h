#pragma once
#include <cstdint>
#include <esp_err.h>
using intr_handler_t=void(*)(void*);
using intr_handle_t=void*;
#define ESP_INTR_FLAG_LEVEL1 (1<<1)
#define ESP_INTR_FLAG_LEVEL2 (1<<2)
#define ESP_INTR_FLAG_LEVEL3 (1<<3)
#define ESP_INTR_FLAG_LEVEL4 (1<<4)
#define ESP_INTR_FLAG_LEVEL5 (1<<5)
#define ESP_INTR_FLAG_LEVEL6 (1<<6)
#define ESP_INTR_FLAG_NMI (1<<7)
#define ESP_INTR_FLAG_SHARED (1<<8)
#define ESP_INTR_FLAG_EDGE (1<<9)
#define ESP_INTR_FLAG_IRAM (1<<10)
#define ESP_INTR_FLAG_INTRDISABLED (1<<11)
#define ESP_INTR_FLAG_LOWMED (ESP_INTR_FLAG_LEVEL1|ESP_INTR_FLAG_LEVEL2|ESP_INTR_FLAG_LEVEL3)
#define ESP_INTR_FLAG_LEVELMASK (ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_LEVEL4|ESP_INTR_FLAG_LEVEL5|ESP_INTR_FLAG_LEVEL6|ESP_INTR_FLAG_NMI)
extern "C" esp_err_t esp_intr_alloc_intrstatus(int,int,uint32_t,uint32_t,intr_handler_t,void*,intr_handle_t*);
extern "C" int esp_intr_get_cpu(intr_handle_t);
extern "C" int esp_intr_get_intno(intr_handle_t);
extern "C" esp_err_t esp_intr_free(intr_handle_t);
namespace IntrFixture{
 extern int group,pair,cpu,vector,source,flags;
 extern unsigned calls,freed;
 extern uint32_t reg,mask;
 extern intr_handler_t handler;
 extern void* arg;
 extern esp_err_t groupResult,result;
 extern bool nullResult;
}
