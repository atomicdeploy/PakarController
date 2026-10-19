#pragma once
#include <stddef.h>
#include <string.h>
#define PROGMEM
inline size_t pakarPropertyFlashReads=0;
inline void *memcpy_P(void *destination,const void *source,size_t length){++pakarPropertyFlashReads;return memcpy(destination,source,length);}
