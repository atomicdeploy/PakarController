#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
constexpr uint32_t MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2;
inline size_t cloudTestInternalFree[2]={60000,42000};
inline size_t cloudTestInternalLargest[2]={31000,18000};
inline unsigned cloudTestFreeReads=0,cloudTestLargestReads=0;
inline size_t heap_caps_get_free_size(uint32_t caps){
  assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  return cloudTestInternalFree[cloudTestFreeReads++%2];
}
inline size_t heap_caps_get_largest_free_block(uint32_t caps){
  assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  return cloudTestInternalLargest[cloudTestLargestReads++%2];
}
