#include "LedDmaGuard.h"
#include <sdkconfig.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_intr_alloc.h>
#include <esp_private/gdma.h>
#include <soc/gdma_periph.h>
#include <xtensa/hal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>

// Adapt the reviewed IDF5.5.5 rmt_tx.c::rmt_tx_init_dma_link allocation flag,
// not private driver layouts or SDK-wide configuration. The shipping SDK
// already places the normal refill graph and its objects in internal memory;
// CONFIG_RMT_TX_ISR_CACHE_SAFE being absent omits this allocation flag only.
// Keep the existing flash guards: this is not a zero-refill DMA implementation.
namespace {
#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5,5,5) && \
    CONFIG_IDF_TARGET_ESP32S3 && CONFIG_RMT_ENCODER_FUNC_IN_IRAM && \
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
bool irqAttempted=false,irqSuccessful=false;
int expectedSource=-1;
PakarS3::LedDmaIrqReceipt pendingReceipt,publishedReceipt;
constexpr int expectedFlags=ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_INTRDISABLED|ESP_INTR_FLAG_LOWMED;
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
  irqAttempted=irqSuccessful=false;expectedSource=-1;pendingReceipt={};
}
LedDmaSetupScope::~LedDmaSetupScope(){
  if(!active_)return;
  std::lock_guard<std::mutex> lock(setupMutex);
  if(setupOwner==this){setupOwner=nullptr;setupTask=nullptr;attempted=successful=poisoned=false;
    irqAttempted=irqSuccessful=false;expectedSource=-1;pendingReceipt={};}
}
bool LedDmaSetupScope::available()const{
  if(!active_ || !taskContext() || xTaskGetCurrentTaskHandle()!=task_)return false;
  std::lock_guard<std::mutex> lock(setupMutex);
  return setupOwner==this && !poisoned;
}
bool LedDmaSetupScope::admitted()const{
  if(!active_ || !taskContext() || xTaskGetCurrentTaskHandle()!=task_)return false;
  std::lock_guard<std::mutex> lock(setupMutex);
  return setupOwner==this && attempted && successful && irqAttempted && irqSuccessful && !poisoned;
}
LedDmaIrqReceipt ledDmaIrqReceipt(){std::lock_guard<std::mutex> lock(setupMutex);return publishedReceipt;}
esp_err_t newCacheSafeLedChannel(const rmt_tx_channel_config_t* config,rmt_channel_handle_t* channel){
  if(!config || !channel || !config->flags.with_dma)return ESP_ERR_INVALID_ARG;
  *channel=nullptr;
  LedDmaSetupScope scope;if(!scope.available())return ESP_ERR_NOT_SUPPORTED;
  {std::lock_guard<std::mutex> lock(setupMutex);publishedReceipt={};}
  const auto result=rmt_new_tx_channel(config,channel);
  if(result!=ESP_OK)return result;
  if(scope.admitted() && *channel){std::lock_guard<std::mutex> lock(setupMutex);publishedReceipt=pendingReceipt;return ESP_OK;}
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
  bool accepted=result==ESP_OK && *channel;
  int group=-1,pair=-1,source=-1;
  if(accepted){
    accepted=gdma_get_group_channel_id(*channel,&group,&pair)==ESP_OK && group>=0 &&
        static_cast<unsigned>(group)<SOC_GDMA_NUM_GROUPS_MAX && pair>=0 && pair<SOC_GDMA_PAIRS_PER_GROUP_MAX;
    if(accepted){source=gdma_periph_signals.groups[group].pairs[pair].tx_irq_id;accepted=source>=0;}
  }
  {
    std::lock_guard<std::mutex> lock(setupMutex);
    if(setupOwner==owner){successful=accepted;expectedSource=source;if(!accepted)poisoned=true;}
  }
  // Leave a successful allocation with the SDK on metadata mismatch. Its
  // normal RMT cleanup can retire it; no channel is admitted without the IRQ.
  // Returning a new failure here would bypass rmt_tx_init_dma_link cleanup.
  if(result==ESP_OK && *channel && !accepted)return ESP_OK;
  return result==ESP_OK && !accepted?ESP_ERR_INVALID_STATE:result;
}
extern "C" esp_err_t __real_esp_intr_alloc_intrstatus(int,int,uint32_t,uint32_t,intr_handler_t,void*,intr_handle_t*);
extern "C" esp_err_t __wrap_esp_intr_alloc_intrstatus(int source,int flags,uint32_t statusReg,
    uint32_t statusMask,intr_handler_t handler,void* arg,intr_handle_t* resultHandle){
  const PakarS3::LedDmaSetupScope* owner=nullptr;
  if(prerequisites && taskContext()){
    std::lock_guard<std::mutex> lock(setupMutex);
    if(setupOwner && setupTask==xTaskGetCurrentTaskHandle() && successful && source==expectedSource){
      if(poisoned || irqAttempted || flags!=expectedFlags || !handler || !resultHandle){poisoned=true;return ESP_ERR_INVALID_STATE;}
      irqAttempted=true;owner=setupOwner;
    }
  }
  if(!owner)return __real_esp_intr_alloc_intrstatus(source,flags,statusReg,statusMask,handler,arg,resultHandle);
  // IDF5.5.5 gdma_install_tx_interrupt requests LOWMED independently of the
  // RMT channel's priority. Preserve every other allocation argument/flag.
  const int forced=(flags&~ESP_INTR_FLAG_LEVELMASK)|ESP_INTR_FLAG_LEVEL3;
  *resultHandle=nullptr;
  const auto result=__real_esp_intr_alloc_intrstatus(source,forced,statusReg,statusMask,handler,arg,resultHandle);
  PakarS3::LedDmaIrqReceipt receipt;
  receipt.source=source;
  if(result==ESP_OK && *resultHandle){
    receipt.cpu=esp_intr_get_cpu(*resultHandle);receipt.vector=esp_intr_get_intno(*resultHandle);
    if(receipt.vector>=0 && receipt.vector<XCHAL_NUM_INTERRUPTS)receipt.level=Xthal_intlevel[receipt.vector];
    receipt.witnessed=receipt.cpu==1 && receipt.level==3;
  }
  {
    std::lock_guard<std::mutex> lock(setupMutex);
    if(setupOwner==owner){pendingReceipt=receipt;irqSuccessful=receipt.witnessed;if(!irqSuccessful)poisoned=true;}
  }
  // The SDK has not enabled or taken ownership of this disabled handle yet.
  // Retire a mismatched measured allocation now; never retry a weaker level.
  if(result==ESP_OK && !receipt.witnessed){
    if(*resultHandle){const auto removed=esp_intr_free(*resultHandle);if(removed!=ESP_OK)return removed;*resultHandle=nullptr;}
    return ESP_ERR_INVALID_STATE;
  }
  return result;
}
