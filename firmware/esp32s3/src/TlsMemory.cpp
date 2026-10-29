#include <esp_heap_caps.h>
#include <stdint.h>
#include <stddef.h>

// ESP-IDF's esp_mem.c allocates every mbedTLS object from internal RAM in the
// bundled Arduino SDK. Two verified TLS connections otherwise exhaust it even
// with 8 MiB PSRAM free. Adapt the SDK's supported SPIRAM allocation strategy
// only for large (record-sized) allocations; small crypto objects retain the
// original allocator. No SDK configuration/ABI, record length or trust changes.
// esp_mbedtls_mem_free already uses heap_caps_free for either memory region.
// GNU --wrap is project-local, as with DnsCoreLock.cpp. With no initialized
// PSRAM (or on allocation failure), preserve the exact original allocation.
extern "C" void *__real_esp_mbedtls_mem_calloc(size_t n,size_t size);
extern "C" void *__wrap_esp_mbedtls_mem_calloc(size_t n,size_t size) {
  if(size && n>SIZE_MAX/size)return nullptr;
  const size_t bytes=n*size;
  constexpr uint32_t external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
  if(bytes>=16384 && heap_caps_get_total_size(external)) {
    if(void *memory=heap_caps_calloc(n,size,external))return memory;
  }
  return __real_esp_mbedtls_mem_calloc(n,size);
}
