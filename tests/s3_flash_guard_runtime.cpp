#include <esp_partition.h>
#include <esp_image_format.h>
#include <atomic>
namespace FlashGuardFixture {
extern std::atomic<bool> nest;
extern esp_err_t realResult;
void realCheck(const esp_partition_t *,size_t,const void *,size_t);
void realCheckImage(esp_image_load_mode_t,const esp_partition_pos_t *,esp_image_metadata_t *);
}
esp_err_t fixtureNestedWrite(const esp_partition_t *,size_t,const void *,size_t);
esp_err_t fixtureNestedRead();
extern "C" esp_err_t esp_partition_write(const esp_partition_t *part,size_t offset,const void *source,size_t bytes){
  FlashGuardFixture::realCheck(part,offset,source,bytes);
  if(FlashGuardFixture::nest.exchange(false)) {
    const auto nested=fixtureNestedWrite(part,offset,source,bytes);
    if(nested!=0)return nested;
  }
  return FlashGuardFixture::realResult;
}
extern "C" esp_err_t esp_partition_write_raw(const esp_partition_t *part,size_t offset,const void *source,size_t bytes){
  FlashGuardFixture::realCheck(part,offset,source,bytes);return FlashGuardFixture::realResult;
}
extern "C" esp_err_t esp_partition_erase_range(const esp_partition_t *part,size_t offset,size_t bytes){
  FlashGuardFixture::realCheck(part,offset,nullptr,bytes);return FlashGuardFixture::realResult;
}
extern "C" esp_err_t esp_partition_read(const esp_partition_t *part,size_t offset,void *out,size_t bytes){
  FlashGuardFixture::realCheck(part,offset,out,bytes);return FlashGuardFixture::realResult;
}
extern "C" esp_err_t esp_partition_read_raw(const esp_partition_t *part,size_t offset,void *out,size_t bytes){
  FlashGuardFixture::realCheck(part,offset,out,bytes);return FlashGuardFixture::realResult;
}
extern "C" esp_err_t esp_image_verify(esp_image_load_mode_t mode,const esp_partition_pos_t *pos,esp_image_metadata_t *metadata){
  FlashGuardFixture::realCheckImage(mode,pos,metadata);
  if(FlashGuardFixture::nest.exchange(false))return fixtureNestedRead();
  return FlashGuardFixture::realResult;
}
