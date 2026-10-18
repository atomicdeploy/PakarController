#pragma once
#include <esp_err.h>
enum gdma_channel_direction_t{GDMA_CHANNEL_DIRECTION_TX,GDMA_CHANNEL_DIRECTION_RX};
using gdma_channel_handle_t=void*;
struct gdma_channel_alloc_config_t{
 gdma_channel_handle_t sibling_chan=nullptr;
 gdma_channel_direction_t direction=GDMA_CHANNEL_DIRECTION_TX;
 struct{unsigned reserve_sibling:1;unsigned isr_cache_safe:1;}flags{};
};
extern "C" esp_err_t gdma_new_ahb_channel(const gdma_channel_alloc_config_t*,gdma_channel_handle_t*);
namespace DmaFixture {
 extern thread_local void* task;
 extern thread_local bool isr,canYield;
 extern thread_local int scheduler;
 extern unsigned calls;
 extern bool safe,reserve,sibling,nullResult;
 extern esp_err_t allocationResult;
}
