#pragma once
#include <stddef.h>
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2;
inline size_t freeInternal=100000,largestInternal=40000;
inline size_t heap_caps_get_free_size(int){return freeInternal;}
inline size_t heap_caps_get_largest_free_block(int){return largestInternal;}
inline size_t heap_caps_get_minimum_free_size(int){return 216;}
