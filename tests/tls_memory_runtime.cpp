#include <stddef.h>
unsigned originalCalls=0;
char internalMemory;
extern "C" void *esp_mbedtls_mem_calloc(size_t,size_t){++originalCalls;return &internalMemory;}
