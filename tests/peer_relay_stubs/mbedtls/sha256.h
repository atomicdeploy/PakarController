#pragma once
#include <stdint.h>
#include <string.h>
// Non-cryptographic fixture stand-in; SDK mbedTLS supplies production SHA256.
// Distinct raw requests must still have distinct fixture fingerprints.
inline int mbedtls_sha256(const unsigned char *raw,size_t n,unsigned char out[32],int){
  memset(out,0,32);for(size_t i=0;i<n;++i)out[i%32]=uint8_t((out[i%32]*33U)^raw[i]);return 0;
}
