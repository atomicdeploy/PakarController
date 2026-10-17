#pragma once
#include <stddef.h>
#include <stdint.h>
using esp_err_t=int;
constexpr esp_err_t ESP_OK=0,ESP_ERR_TIMEOUT=0x107;
struct esp_partition_t {bool encrypted;};
extern "C" esp_err_t esp_partition_write(const esp_partition_t *,size_t,const void *,size_t);
extern "C" esp_err_t esp_partition_write_raw(const esp_partition_t *,size_t,const void *,size_t);
extern "C" esp_err_t esp_partition_erase_range(const esp_partition_t *,size_t,size_t);
extern "C" esp_err_t esp_partition_read(const esp_partition_t *,size_t,void *,size_t);
extern "C" esp_err_t esp_partition_read_raw(const esp_partition_t *,size_t,void *,size_t);
