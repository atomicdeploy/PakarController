#include <esp_partition.h>
#include <esp_image_format.h>
#include "../firmware/esp32s3/src/LedFlashGuard.h"
#include <esp_private/cache_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cassert>
#include <cstdio>
#include <atomic>
#include <thread>

namespace FlashGuardFixture {
inline std::atomic<unsigned> acquired{0},released{0},holds{0},realCalls{0};
inline std::atomic<bool> allow{true},nest{false};
inline esp_err_t realResult=ESP_OK;
inline const esp_partition_t *expectedPartition=nullptr;
inline const void *expectedSource=nullptr;
inline size_t expectedOffset=0,expectedBytes=0;
inline esp_partition_pos_t position{0x210000,0x200000};
inline esp_image_metadata_t metadata{};
void realCheck(const esp_partition_t *part,size_t offset,const void *source,size_t bytes){
  assert(holds.load()>0);assert(part==expectedPartition && offset==expectedOffset && bytes==expectedBytes);
  assert(!source || source==expectedSource);++realCalls;
}
void realCheckImage(esp_image_load_mode_t mode,const esp_partition_pos_t *pos,esp_image_metadata_t *data){
  assert(holds.load()>0 && mode==ESP_IMAGE_VERIFY_SILENT && pos==&position && data==&metadata);++realCalls;
}
}
namespace PakarS3 {
bool acquireLedFlashPause(){using namespace FlashGuardFixture;++acquired;if(!allow.load())return false;++holds;return true;}
void releaseLedFlashPause(){using namespace FlashGuardFixture;assert(holds.fetch_sub(1)>0);++released;}
}
esp_err_t fixtureNestedWrite(const esp_partition_t *part,size_t offset,const void *source,size_t bytes){
  // Separate translation unit from the SDK-like definitions, so GNU --wrap
  // intercepts this call as it does actual externally referenced SDK APIs.
  return esp_partition_write_raw(part,offset,source,bytes);
}
esp_err_t fixtureNestedRead(){
  using namespace FlashGuardFixture;
  return esp_partition_read_raw(expectedPartition,expectedOffset,const_cast<void *>(expectedSource),expectedBytes);
}
int main(){
  using namespace FlashGuardFixture;
  const esp_partition_t plain{false},encrypted{true};uint8_t source[]={1,2,3,4};
  expectedPartition=&plain;expectedOffset=16;expectedBytes=sizeof(source);expectedSource=source;
  assert(esp_partition_write(&plain,16,source,sizeof(source))==ESP_OK);
  assert(realCalls==1 && acquired==1 && released==1 && holds==0);
  expectedPartition=&encrypted;realResult=0x102;
  assert(esp_partition_write(&encrypted,16,source,sizeof(source))==realResult);
  assert(realCalls==2 && acquired==2 && released==2 && holds==0);
  expectedPartition=&plain;realResult=ESP_OK;
  assert(esp_partition_write_raw(&plain,16,source,sizeof(source))==ESP_OK);
  assert(esp_partition_erase_range(&plain,16,sizeof(source))==ESP_OK);
  assert(realCalls==4 && released==4 && holds==0);
  const auto before=realCalls.load();allow=false;
  assert(esp_partition_write(&plain,16,source,sizeof(source))==ESP_ERR_TIMEOUT);
  assert(esp_partition_write_raw(&plain,16,source,sizeof(source))==ESP_ERR_TIMEOUT);
  assert(esp_partition_erase_range(&plain,16,sizeof(source))==ESP_ERR_TIMEOUT);
  assert(realCalls==before && acquired==7 && released==4 && holds==0);allow=true;
  for(unsigned unsafe=0;unsafe<4;++unsafe){
    cacheEnabled=unsafe!=0;isr=unsafe==1;canYield=unsafe!=2;
    scheduler=unsafe==3?taskSCHEDULER_SUSPENDED:taskSCHEDULER_RUNNING;
    assert(esp_partition_write(&plain,16,source,sizeof(source))==ESP_ERR_TIMEOUT);
    assert(esp_partition_write_raw(&plain,16,source,sizeof(source))==ESP_ERR_TIMEOUT);
    assert(esp_partition_erase_range(&plain,16,sizeof(source))==ESP_ERR_TIMEOUT);
  }
  assert(realCalls==before && acquired==7 && released==4);
  cacheEnabled=true;isr=false;canYield=true;scheduler=taskSCHEDULER_NOT_STARTED;
  assert(esp_partition_write(&plain,16,source,sizeof(source))==ESP_OK);
  scheduler=taskSCHEDULER_RUNNING;nest=true;
  assert(esp_partition_write(&plain,16,source,sizeof(source))==ESP_OK);
  assert(realCalls==before+3 && acquired==10 && released==7 && holds==0);
  std::thread first([&]{for(unsigned i=0;i<1000;++i)assert(esp_partition_write(&plain,16,source,sizeof(source))==ESP_OK);});
  std::thread second([&]{for(unsigned i=0;i<1000;++i)assert(esp_partition_erase_range(&plain,16,sizeof(source))==ESP_OK);});
  first.join();second.join();assert(acquired==2010 && released==2007 && holds==0 && realCalls==2007);
  auto readAndVerify=[&]{
    const auto expected=allow.load()&&cacheEnabled&&!isr&&canYield&&scheduler!=taskSCHEDULER_SUSPENDED?realResult:ESP_ERR_TIMEOUT;
    assert(esp_partition_read(&plain,16,source,sizeof(source))==expected);
    assert(esp_partition_read_raw(&plain,16,source,sizeof(source))==expected);
    assert(esp_image_verify(ESP_IMAGE_VERIFY_SILENT,&position,&metadata)==expected);
  };
  auto calls=realCalls.load(),releases=released.load();readAndVerify();
  assert(realCalls==calls+3 && released==releases+3 && holds==0);
  realResult=0x103;readAndVerify();assert(realCalls==calls+6 && released==releases+6 && holds==0);realResult=ESP_OK;
  calls=realCalls.load();releases=released.load();allow=false;readAndVerify();allow=true;
  assert(realCalls==calls && released==releases && holds==0);
  for(unsigned unsafe=0;unsafe<4;++unsafe){
    cacheEnabled=unsafe!=0;isr=unsafe==1;canYield=unsafe!=2;
    scheduler=unsafe==3?taskSCHEDULER_SUSPENDED:taskSCHEDULER_RUNNING;
    const auto acquisitions=acquired.load();readAndVerify();
    assert(realCalls==calls && released==releases && acquired==acquisitions && holds==0);
  }
  cacheEnabled=true;isr=false;canYield=true;scheduler=taskSCHEDULER_NOT_STARTED;readAndVerify();
  scheduler=taskSCHEDULER_RUNNING;calls=realCalls.load();releases=released.load();nest=true;
  assert(esp_image_verify(ESP_IMAGE_VERIFY_SILENT,&position,&metadata)==ESP_OK);
  assert(realCalls==calls+2 && released==releases+2 && holds==0);
  {
    PakarS3::ScopedLedFlashPause outer;assert(outer && holds==1);readAndVerify();assert(holds==1);
  }
  assert(holds==0);allow=false;{
    PakarS3::ScopedLedFlashPause denied;assert(!denied && holds==0);
  }allow=true;assert(holds==0);
  std::thread reader([&]{for(unsigned i=0;i<1000;++i)readAndVerify();});
  std::thread verifier([&]{for(unsigned i=0;i<1000;++i)readAndVerify();});
  reader.join();verifier.join();assert(holds==0);
  puts("Actual linked partition write/raw/erase/read/read_raw and image-verifier wrappers: fail-closed acquisition, preserved errors, nested verification/read and scoped metadata holds, unsafe-context rejection and concurrent calls passed; not SDK flash/DMA hardware proof.");
}
