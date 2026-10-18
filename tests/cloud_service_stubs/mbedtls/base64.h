#pragma once
#include <stddef.h>
#include <string.h>
// Serialization/dispatch fixture only; not a crypto/base64 implementation test.
inline int mbedtls_base64_encode(unsigned char*out,size_t cap,size_t*n,const unsigned char*,size_t bytes){*n=(bytes+2)/3*4;if(*n>=cap)return -1;memset(out,'A',*n);return 0;}
