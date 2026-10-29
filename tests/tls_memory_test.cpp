#include <assert.h>
#include <stdint.h>
#include <esp_heap_caps.h>
extern unsigned originalCalls;
extern char internalMemory;
extern "C" void *esp_mbedtls_mem_calloc(size_t,size_t);
static unsigned externalCalls=0;
static size_t capacity=0,lastN=0,lastSize=0;
static bool fail=false;
static char externalMemory;
size_t heap_caps_get_total_size(uint32_t caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));return capacity;}
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++externalCalls;lastN=n;lastSize=size;return fail?nullptr:&externalMemory;}
int main(){
 assert(esp_mbedtls_mem_calloc(1,16469)==&internalMemory&&originalCalls==1&&externalCalls==0);
 capacity=8U*1024*1024;
 assert(esp_mbedtls_mem_calloc(1,16383)==&internalMemory&&externalCalls==0);
 assert(esp_mbedtls_mem_calloc(1,16384)==&externalMemory&&externalCalls==1&&originalCalls==2);
 assert(esp_mbedtls_mem_calloc(2,9000)==&externalMemory&&lastN==2&&lastSize==9000);
 fail=true;assert(esp_mbedtls_mem_calloc(1,16469)==&internalMemory&&originalCalls==3);
 assert(esp_mbedtls_mem_calloc(SIZE_MAX,2)==nullptr&&originalCalls==3&&externalCalls==3);
 assert(esp_mbedtls_mem_calloc(0,0)==&internalMemory&&originalCalls==4);
}
