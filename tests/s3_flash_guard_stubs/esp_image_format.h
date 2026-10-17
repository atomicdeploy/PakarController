#pragma once
#include <esp_partition.h>
enum esp_image_load_mode_t {ESP_IMAGE_VERIFY,ESP_IMAGE_VERIFY_SILENT};
struct esp_partition_pos_t {uint32_t offset,size;};
struct esp_image_metadata_t {uint32_t image_len;};
extern "C" esp_err_t esp_image_verify(esp_image_load_mode_t,const esp_partition_pos_t *,esp_image_metadata_t *);
