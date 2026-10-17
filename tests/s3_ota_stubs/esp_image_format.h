#pragma once
#include "esp_ota_ops.h"
struct esp_partition_pos_t {uint32_t offset,size;};
struct esp_image_metadata_t {uint32_t image_len;};
constexpr int ESP_IMAGE_VERIFY=0,ESP_CHIP_ID_ESP32S3=9;
struct __attribute__((packed)) esp_image_header_t {
 uint8_t magic,segments,mode,speed;uint32_t entry;uint8_t pins[4];
 uint16_t chip_id;uint8_t revision;uint16_t min_chip_rev_full,max_chip_rev_full;
 uint8_t reserved[4],hash_appended;
};
static_assert(sizeof(esp_image_header_t)==24);
inline int esp_image_verify(int,const esp_partition_pos_t *p,esp_image_metadata_t *m){
 auto &v=OtaFixture::images[p->offset];if(OtaFixture::failVerify || v.size()<24)return -1;
 m->image_len=v.size();return ESP_OK;
}
