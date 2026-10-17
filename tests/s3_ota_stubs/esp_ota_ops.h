#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <vector>
using esp_err_t=int;using esp_ota_handle_t=uint32_t;
constexpr int ESP_OK=0;
constexpr size_t OTA_WITH_SEQUENTIAL_WRITES=0xfffffffe;
struct esp_partition_t {uint32_t address,size;};
namespace OtaFixture {
inline esp_partition_t partitions[]={{0x10000,0x330000},{0x340000,0x330000}};
inline unsigned running=0,boot=0,aborts=0,ends=0,selections=0,begins=0;
inline bool active=false,failBegin=false,failWrite=false,failEnd=false,failRead=false,failSelect=false,failVerify=false;
inline std::map<uint32_t,std::vector<uint8_t>> images;
inline void reset(){running=boot=aborts=ends=selections=begins=0;active=failBegin=failWrite=failEnd=failRead=failSelect=failVerify=false;images.clear();}
inline void reboot(){running=boot;}
}
inline const esp_partition_t *esp_ota_get_running_partition(){return &OtaFixture::partitions[OtaFixture::running];}
inline const esp_partition_t *esp_ota_get_boot_partition(){return &OtaFixture::partitions[OtaFixture::boot];}
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t*){return &OtaFixture::partitions[1-OtaFixture::running];}
inline int esp_ota_begin(const esp_partition_t *p,size_t mode,esp_ota_handle_t *handle){
 if(OtaFixture::failBegin || OtaFixture::active || mode!=OTA_WITH_SEQUENTIAL_WRITES || p==esp_ota_get_running_partition())return -1;
 OtaFixture::active=true;++OtaFixture::begins;*handle=p->address;OtaFixture::images[p->address].clear();return ESP_OK;
}
inline int esp_ota_write(esp_ota_handle_t h,const void *data,size_t n){
 if(!OtaFixture::active || OtaFixture::failWrite)return -1;
 const auto *p=static_cast<const uint8_t*>(data);auto &v=OtaFixture::images[h];v.insert(v.end(),p,p+n);return ESP_OK;
}
inline int esp_ota_end(esp_ota_handle_t){OtaFixture::active=false;++OtaFixture::ends;return OtaFixture::failEnd?-1:ESP_OK;}
inline int esp_ota_abort(esp_ota_handle_t){OtaFixture::active=false;++OtaFixture::aborts;return ESP_OK;}
inline int esp_ota_set_boot_partition(const esp_partition_t *p){
 if(OtaFixture::failSelect)return -1;
 OtaFixture::boot=p->address==OtaFixture::partitions[0].address?0:1;++OtaFixture::selections;return ESP_OK;
}
inline int esp_partition_read(const esp_partition_t *p,size_t offset,void *out,size_t bytes){
 const auto &v=OtaFixture::images[p->address];if(OtaFixture::failRead || offset>v.size() || bytes>v.size()-offset)return -1;
 memcpy(out,v.data()+offset,bytes);return ESP_OK;
}
