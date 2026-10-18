#pragma once
#include <stddef.h>
#include <string.h>
// Serialization/dispatch fixture only; actual hash primitives tested elsewhere.
inline int mbedtls_sha256(const unsigned char*,size_t,unsigned char out[32],int){memset(out,0,32);return 0;}
