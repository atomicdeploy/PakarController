#include "LedDmaGuard.h"
#include <sdkconfig.h>
#include <esp_err.h>
#include <esp_private/gdma.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>

// Adapt the reviewed IDF5.5.5 rmt_tx.c::rmt_tx_init_dma_link allocation flag,
// not private driver layouts or SDK-wide configuration. The shipping SDK
// already places the normal refill graph and its objects in internal memory;
// CONFIG_RMT_TX_ISR_CACHE_SAFE being absent omits this allocation flag only.
// Keep the existing flash guards: this is not a zero-refill DMA implementation.
namespace {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_RMT_ENCODER_FUNC_IN_IRAM && \
    CONFIG_RMT_TX_ISR_HANDLER_IN_IRAM && CONFIG_RMT_OBJ_CACHE_SAFE && \
    CONFIG_GDMA_CTRL_FUNC_IN_IRAM && CONFIG_GDMA_ISR_HANDLER_IN_IRAM && \
    CONFIG_GDMA_OBJ_DRAM_SAFE
constexpr bool prerequisites=true;
#else
constexpr bool prerequisites=false;
#endif
std::mutex setupMutex;
const PakarS3::LedDmaSetupScope* setupOwner=nullptr;
TaskHandle_t setupTask=nullptr;
bool attempted=false,successful=false,poisoned=false;
bool taskContext(){
  return !xPortInIsrContext() && xPortCanYield() &&
      xTaskGetSchedulerState()==taskSCHEDULER_RUNNING;
}
}
namespace PakarS3 {
LedDmaSetupScope::LedDmaSetupScope(){
  if(!prerequisites || !taskContext())return;
  const auto task=xTaskGetCurrentTaskHandle();if(!task)return;
  std::lock_guard<std::mutex> lock(setupMutex);
  if(setupOwner)return; // Nested/other-task scopes cannot replace the owner.
  task_=task;active_=true;setupOwner=this;setupTask=task;
  attempted=successful=poisoned=false;
}
LedDmaSetupScope::~LedDmaSetupScope(){
  if(!active_)return;
  std::lock_guard<std::mutex> lock(setupMutex);
  if(setupOwner==this){setupOwner=nullptr;setupTask=nullptr;attempted=successful=poisoned=false;}
}
bool LedDmaSetupScope::available()const{
  if(!active_ || !taskContext() || xTaskGetCurrentTaskHandle()!=task_)return false;
  std::lock_guard<std::mutex> lock(setupMutex);
  return setupOwner==this && !poisoned;
}
bool LedDmaSetupScope::admitted()const{
  if(!active_ || !taskContext() || xTaskGetCurrentTaskHandle()!=task_)return false;
  std::lock_guard<std::mutex> lock(setupMutex);
  return setupOwner==this && attempted && successful && !poisoned;
}
esp_err_t newCacheSafeLedChannel(const rmt_tx_channel_config_t* config,rmt_channel_handle_t* channel){
  if(!config || !channel || !config->flags.with_dma)return ESP_ERR_INVALID_ARG;
  *channel=nullptr;
  LedDmaSetupScope scope;if(!scope.available())return ESP_ERR_NOT_SUPPORTED;
  const auto result=rmt_new_tx_channel(config,channel);
  if(result!=ESP_OK)return result;
  if(scope.admitted() && *channel)return ESP_OK;
  // A changed SDK or omitted GNU wrap must not silently create an unsafe
  // channel. It has not been enabled; retire it before reporting failure.
  if(*channel){const auto removed=rmt_del_channel(*channel);if(removed!=ESP_OK)return removed;*channel=nullptr;}
  return ESP_ERR_INVALID_STATE;
}
}
extern "C" esp_err_t __real_gdma_new_ahb_channel(
    const gdma_channel_alloc_config_t*,gdma_channel_handle_t*);
extern "C" esp_err_t __wrap_gdma_new_ahb_channel(
    const gdma_channel_alloc_config_t* config,gdma_channel_handle_t* channel){
  const PakarS3::LedDmaSetupScope* owner=nullptr;
  // Nonmatching callers, including RX and unrelated tasks, keep exact SDK
  // semantics. No lock is held across the real allocator or SDK callbacks.
  if(prerequisites && config && channel && config->direction==GDMA_CHANNEL_DIRECTION_TX && taskContext()){
    std::lock_guard<std::mutex> lock(setupMutex);
    if(setupOwner && setupTask==xTaskGetCurrentTaskHandle()){
      if(attempted){poisoned=true;return ESP_ERR_INVALID_STATE;}
      attempted=true;owner=setupOwner;
    }
  }
  if(!owner)return __real_gdma_new_ahb_channel(config,channel);
  auto forced=*config;forced.flags.isr_cache_safe=1;*channel=nullptr;
  const auto result=__real_gdma_new_ahb_channel(&forced,channel);
  const bool accepted=result==ESP_OK && *channel;
  {
    std::lock_guard<std::mutex> lock(setupMutex);
    if(setupOwner==owner){successful=accepted;if(!accepted)poisoned=true;}
  }
  return result==ESP_OK && !accepted?ESP_ERR_INVALID_STATE:result;
}
