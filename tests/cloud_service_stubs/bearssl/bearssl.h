#pragma once
#include <stddef.h>
#include <string.h>
// Dispatch/serialization fixture only, not a BearSSL primitive test.
struct br_sha256_context {};
inline void br_sha256_init(br_sha256_context*){}
inline void br_sha256_update(br_sha256_context*,const void*,size_t){}
inline void br_sha256_out(const br_sha256_context*,void*out){memset(out,0,32);}
