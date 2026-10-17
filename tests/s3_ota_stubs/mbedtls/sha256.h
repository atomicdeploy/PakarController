#pragma once
#include "../../remote_stubs/bearssl/bearssl.h"
using mbedtls_sha256_context=br_sha256_context;
inline void mbedtls_sha256_init(mbedtls_sha256_context*c){br_sha256_init(c);}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}
inline void mbedtls_sha256_clone(mbedtls_sha256_context*c,const mbedtls_sha256_context*s){*c=*s;}
inline int mbedtls_sha256_starts(mbedtls_sha256_context*c,int){br_sha256_init(c);return 0;}
inline int mbedtls_sha256_update(mbedtls_sha256_context*c,const uint8_t*p,size_t n){br_sha256_update(c,p,n);return 0;}
inline int mbedtls_sha256_finish(mbedtls_sha256_context*c,uint8_t*out){br_sha256_out(c,out);return 0;}
