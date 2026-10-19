#include <esp_private/gdma.h>
#include <freertos/task.h>
#include <esp_intr_alloc.h>
#include <cassert>
namespace DmaFixture {
 thread_local void* task=reinterpret_cast<void*>(1);
 thread_local bool isr=false,canYield=true;
 thread_local int scheduler=taskSCHEDULER_RUNNING;
 unsigned calls=0;
 bool safe=false,reserve=false,sibling=false,nullResult=false;
 esp_err_t allocationResult=ESP_OK;
}
BaseType_t xPortInIsrContext(){return DmaFixture::isr;}
BaseType_t xPortCanYield(){return DmaFixture::canYield;}
BaseType_t xTaskGetSchedulerState(){return DmaFixture::scheduler;}
TaskHandle_t xTaskGetCurrentTaskHandle(){return DmaFixture::task;}
extern "C" esp_err_t gdma_new_ahb_channel(const gdma_channel_alloc_config_t* config,gdma_channel_handle_t* channel){
 ++DmaFixture::calls;
 if(!config||!channel)return ESP_ERR_INVALID_ARG;
 DmaFixture::safe=config->flags.isr_cache_safe;
 DmaFixture::reserve=config->flags.reserve_sibling;
 DmaFixture::sibling=config->sibling_chan==reinterpret_cast<void*>(7);
 if(DmaFixture::allocationResult!=ESP_OK)return DmaFixture::allocationResult;
 *channel=DmaFixture::nullResult?nullptr:reinterpret_cast<void*>(3);return ESP_OK;
}
// Deliberately a different translation unit from installIrq(): GNU --wrap
// replaces undefined references, not assembler-local calls to a real stub.
extern "C" esp_err_t esp_intr_alloc_intrstatus(int source,int flags,uint32_t reg,uint32_t mask,intr_handler_t handler,void* arg,intr_handle_t* handle){
 ++IntrFixture::calls;IntrFixture::source=source;IntrFixture::flags=flags;IntrFixture::reg=reg;IntrFixture::mask=mask;IntrFixture::handler=handler;IntrFixture::arg=arg;
 if(!handler||!handle)return ESP_ERR_INVALID_ARG;
 if(IntrFixture::result!=ESP_OK)return IntrFixture::result;
 *handle=IntrFixture::nullResult?nullptr:reinterpret_cast<void*>(9);return ESP_OK;
}
extern "C" int esp_intr_get_cpu(intr_handle_t handle){assert(handle);return IntrFixture::cpu;}
extern "C" int esp_intr_get_intno(intr_handle_t handle){assert(handle);return IntrFixture::vector;}
extern "C" esp_err_t esp_intr_free(intr_handle_t handle){assert(handle);++IntrFixture::freed;return ESP_OK;}
