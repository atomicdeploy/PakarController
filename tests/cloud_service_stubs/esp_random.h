#pragma once
#include <stddef.h>
#include <string.h>
inline void esp_fill_random(void *p,size_t n){memset(p,1,n);}
