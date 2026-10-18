#include "../firmware/esp32s3/src/LedDmaGuard.h"
#include <esp_private/gdma.h>
#include <freertos/task.h>
#include <cassert>
#include <cstdio>
#include <thread>
using PakarS3::LedDmaSetupScope;
using namespace DmaFixture;
namespace RmtFixture{bool bypass=false;esp_err_t result=ESP_OK;unsigned calls=0,removed=0;}
extern "C" esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*,rmt_channel_handle_t* channel){
 ++RmtFixture::calls;
 if(RmtFixture::result!=ESP_OK)return RmtFixture::result;
 if(RmtFixture::bypass){*channel=reinterpret_cast<void*>(4);return ESP_OK;}
 gdma_channel_alloc_config_t config{};return gdma_new_ahb_channel(&config,channel);
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
  assert(!config.flags.isr_cache_safe&&scope.admitted());
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
  assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&scope.admitted());
 }
 isr=true;{LedDmaSetupScope invalid;assert(!invalid.available());}isr=false;
 canYield=false;{LedDmaSetupScope invalid;assert(!invalid.available());}canYield=true;
 scheduler=taskSCHEDULER_SUSPENDED;{LedDmaSetupScope invalid;assert(!invalid.available());}
 scheduler=taskSCHEDULER_NOT_STARTED;{LedDmaSetupScope invalid;assert(!invalid.available());}
 scheduler=taskSCHEDULER_RUNNING;task=nullptr;{LedDmaSetupScope invalid;assert(!invalid.available());}
 task=reinterpret_cast<void*>(1);
 {LedDmaSetupScope valid;assert(valid.available());assert(gdma_new_ahb_channel(&config,&channel)==ESP_OK&&valid.admitted());}
 assert(PakarS3::newCacheSafeLedChannel(&rmt,&channel)==ESP_OK&&channel&&safe);
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
 puts("Actual scoped GDMA wrapper + GNU binding: only owner TX, immutable config, RX/other task preserved, nested/duplicate/failed/invalid setup fail closed.");
#endif
 puts("Host allocator doubles only, not physical waveform/cache-off/Wi-Fi acceptance.");
}
