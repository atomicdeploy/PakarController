#include "../firmware/esp32s3/src/LedDmaGuard.h"
#include <esp_private/gdma.h>
#include <freertos/task.h>
#include <esp_intr_alloc.h>
#include <soc/gdma_periph.h>
#include <xtensa/hal.h>
#include <cassert>
#include <cstdio>
#include <thread>
#include <initializer_list>
using PakarS3::LedDmaSetupScope;
using namespace DmaFixture;
const gdma_signal_conn_t gdma_periph_signals={{{0,{{71,70},{73,72},{75,74},{77,76},{79,78}}}}};
const unsigned char Xthal_intlevel[32]={0,1,1,1,1,3};
namespace IntrFixture{
 int group=0,pair=0,cpu=1,vector=5,source=-1,flags=0;
 unsigned calls=0,freed=0;uint32_t reg=0,mask=0;
 intr_handler_t handler=nullptr;void* arg=nullptr;
 esp_err_t groupResult=ESP_OK,result=ESP_OK;bool nullResult=false;
}
extern "C" esp_err_t gdma_get_group_channel_id(gdma_channel_handle_t handle,int* group,int* pair){
 assert(handle);*group=IntrFixture::group;*pair=IntrFixture::pair;return IntrFixture::groupResult;
}
constexpr int originalFlags=ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_INTRDISABLED|ESP_INTR_FLAG_LOWMED;
void irqHandler(void*){}
esp_err_t installIrq(int source=70,int flags=originalFlags){intr_handle_t handle=nullptr;return esp_intr_alloc_intrstatus(source,flags,1234,0x55,irqHandler,reinterpret_cast<void*>(8),&handle);}
namespace RmtFixture{bool bypass=false,skipIrq=false;esp_err_t result=ESP_OK;unsigned calls=0,removed=0;}
extern "C" esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*,rmt_channel_handle_t* channel){
 ++RmtFixture::calls;
 if(RmtFixture::result!=ESP_OK)return RmtFixture::result;
 if(RmtFixture::bypass){*channel=reinterpret_cast<void*>(4);return ESP_OK;}
 gdma_channel_alloc_config_t config{};const auto result=gdma_new_ahb_channel(&config,channel);
 if(result!=ESP_OK)return result;
 if(!RmtFixture::skipIrq){const auto irq=installIrq();if(irq!=ESP_OK){*channel=nullptr;return irq;}}
 return ESP_OK;
}
extern "C" esp_err_t rmt_del_channel(rmt_channel_handle_t channel){assert(channel);++RmtFixture::removed;return ESP_OK;}
int main(){
 gdma_channel_alloc_config_t config{};config.flags.reserve_sibling=1;config.sibling_chan=reinterpret_cast<void*>(7);
 gdma_channel_handle_t channel=nullptr;
 rmt_tx_channel_config_t rmt{};rmt.flags.with_dma=1;
 assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&!safe&&reserve&&sibling);
#if defined(PAKAR_DMA_GUARD_UNSUPPORTED_TEST)
 LedDmaSetupScope unsupported;assert(!unsupported.available()&&!unsupported.admitted());
 assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&!safe);
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_ERR_NOT_SUPPORTED&&!channel&&!RmtFixture::calls);
 puts("Missing SDK internal-memory prerequisite: scope unavailable, unrelated GDMA unchanged.");
#else
 {
  LedDmaSetupScope scope;assert(scope.available()&&!scope.admitted());
  {LedDmaSetupScope nested;assert(!nested.available()&&!nested.admitted());}
  assert(scope.available());config.direction=GDMA_CHANNEL_DIRECTION_RX;
  assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&!safe&&!scope.admitted());
  config.direction=GDMA_CHANNEL_DIRECTION_TX;
  std::thread other([&]{task=reinterpret_cast<void*>(2);LedDmaSetupScope competing;
   assert(!competing.available());gdma_channel_handle_t one=nullptr;
   assert(gdma_new_ahb_channel(&config,&one)==ESP_OK&&!safe);});other.join();
  assert(!scope.admitted());
  assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&safe&&reserve&&sibling);
  assert(!config.flags.isr_cache_safe&&!scope.admitted());
  assert(installIrq()==ESP_OK&&scope.admitted());
  assert(IntrFixture::flags==((originalFlags&~ESP_INTR_FLAG_LEVELMASK)|ESP_INTR_FLAG_LEVEL3));
  assert(IntrFixture::reg==1234&&IntrFixture::mask==0x55&&IntrFixture::handler==irqHandler&&IntrFixture::arg==reinterpret_cast<void*>(8));
  const auto before=calls;assert(gdma_new_ahb_channel(&config,&channel)==ESP_ERR_INVALID_STATE);
  assert(calls==before&&!scope.admitted()&&!scope.available());
 }
 assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&!safe);
 {
  LedDmaSetupScope scope;allocationResult=ESP_ERR_NO_MEM;
  assert(gdma_new_ahb_channel(&config,&channel)==ESP_ERR_NO_MEM&&!scope.admitted());
  allocationResult=ESP_OK;
 }
 {
  LedDmaSetupScope scope;nullResult=true;
  assert(gdma_new_ahb_channel(&config,&channel)==ESP_ERR_INVALID_STATE&&!scope.admitted());
  nullResult=false;
 }
 {
  LedDmaSetupScope scope;
  assert(gdma_new_ahb_channel(nullptr,&channel)==ESP_ERR_INVALID_ARG&&!scope.admitted());
  assert(gdma_new_ahb_channel(&config,nullptr)==ESP_ERR_INVALID_ARG&&!scope.admitted());
  assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&!scope.admitted());
  assert(installIrq()==ESP_OK&&scope.admitted());
 }
 isr=true;{LedDmaSetupScope invalid;assert(!invalid.available());}isr=false;
 canYield=false;{LedDmaSetupScope invalid;assert(!invalid.available());}canYield=true;
 scheduler=taskSCHEDULER_SUSPENDED;{LedDmaSetupScope invalid;assert(!invalid.available());}
 scheduler=taskSCHEDULER_NOT_STARTED;{LedDmaSetupScope invalid;assert(!invalid.available());}
 scheduler=taskSCHEDULER_RUNNING;task=nullptr;{LedDmaSetupScope invalid;assert(!invalid.available());}
 task=reinterpret_cast<void*>(1);
 {LedDmaSetupScope valid;assert(valid.available());assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK);assert(installIrq()==ESP_OK&&valid.admitted());}
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_OK&&channel&&safe);
 auto receipt=PakarS3::ledDmaIrqReceipt();assert(receipt.witnessed&&receipt.cpu==1&&receipt.vector==5&&receipt.level==3&&receipt.source==70);
 for(int mismatch: {0,1,2,3,4,5}){
  LedDmaSetupScope scope;assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK);const auto before=IntrFixture::calls;
  if(mismatch==0){assert(installIrq(72)==ESP_OK);assert(IntrFixture::flags==originalFlags);}
  if(mismatch==1){assert(installIrq(70,originalFlags|ESP_INTR_FLAG_SHARED)==ESP_ERR_INVALID_STATE);assert(IntrFixture::calls==before);}
  if(mismatch==2){IntrFixture::cpu=0;assert(installIrq()==ESP_ERR_INVALID_STATE);IntrFixture::cpu=1;}
  if(mismatch==3){IntrFixture::vector=4;assert(installIrq()==ESP_ERR_INVALID_STATE);IntrFixture::vector=5;}
  if(mismatch==4){IntrFixture::vector=32;assert(installIrq()==ESP_ERR_INVALID_STATE);IntrFixture::vector=5;}
  if(mismatch==5){IntrFixture::result=ESP_ERR_NO_MEM;assert(installIrq()==ESP_ERR_NO_MEM);IntrFixture::result=ESP_OK;}
  assert(!scope.admitted());
 }
 for(int invalid: {0,1,2}){
  LedDmaSetupScope scope;assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK);intr_handle_t handle=nullptr;
  if(invalid==0){assert(esp_intr_alloc_intrstatus(70,originalFlags,1234,0x55,nullptr,nullptr,&handle)==ESP_ERR_INVALID_STATE);}
  if(invalid==1){assert(esp_intr_alloc_intrstatus(70,originalFlags,1234,0x55,irqHandler,nullptr,nullptr)==ESP_ERR_INVALID_STATE);}
  if(invalid==2){IntrFixture::nullResult=true;assert(installIrq()==ESP_ERR_INVALID_STATE);IntrFixture::nullResult=false;}
  assert(!scope.admitted());
 }
 {
  LedDmaSetupScope scope;assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK);
  std::thread other([&]{task=reinterpret_cast<void*>(2);assert(installIrq()==ESP_OK);assert(IntrFixture::flags==originalFlags);});other.join();
  assert(!scope.admitted());assert(installIrq()==ESP_OK&&scope.admitted());
  const auto before=IntrFixture::calls;assert(installIrq()==ESP_ERR_INVALID_STATE&&!scope.admitted());assert(IntrFixture::calls==before);
 }
 for(int bad: {-1,5}){IntrFixture::pair=bad;LedDmaSetupScope scope;assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK);assert(!scope.admitted());IntrFixture::pair=0;}
 IntrFixture::group=1;{LedDmaSetupScope scope;assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&!scope.admitted());}IntrFixture::group=0;
 RmtFixture::skipIrq=true;const auto beforeMissing=RmtFixture::removed;
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_ERR_INVALID_STATE&&!channel&&RmtFixture::removed==beforeMissing+1);
 assert(!PakarS3::ledDmaIrqReceipt().witnessed);RmtFixture::skipIrq=false;
 RmtFixture::bypass=true;const auto removed=RmtFixture::removed;
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_ERR_INVALID_STATE&&!channel&&RmtFixture::removed==removed+1);
 RmtFixture::bypass=false;RmtFixture::result=ESP_ERR_NO_MEM;
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_ERR_NO_MEM&&!channel);
 RmtFixture::result=ESP_OK;const auto rmtCalls=RmtFixture::calls;
 {LedDmaSetupScope outer;assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_ERR_NOT_SUPPORTED&&!channel);}
 assert(RmtFixture::calls==rmtCalls);
 rmt.flags.with_dma=0;
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_ERR_INVALID_ARG);
 assert(PakarS3::newCacheSafeLedChannel(nullptr,&channel)==ESP_ERR_INVALID_ARG);
 assert(PakarS3::newCacheSafeLedChannel(&rmt,nullptr)==ESP_ERR_INVALID_ARG);
 puts("Scoped GDMA cache-safe + exact refill IRQ3/core1 measured receipt: owner TX only, original flags/args immutable, unrelated IRQs/tasks preserved, missing/mismatched/duplicate/failed allocations rejected.");
#endif
 puts("Host allocator doubles only, not physical waveform/cache-off/Wi-Fi acceptance.");
}
