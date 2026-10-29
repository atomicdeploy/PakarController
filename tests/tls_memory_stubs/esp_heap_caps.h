#pragma once
#include <stddef.h>
#include <stdint.h>
constexpr uint32_t MALLOC_CAP_SPIRAM=1U<<10,MALLOC_CAP_8BIT=1U<<2;
size_t heap_caps_get_total_size(uint32_t caps);
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps);
