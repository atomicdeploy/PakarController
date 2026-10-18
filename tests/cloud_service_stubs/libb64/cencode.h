#pragma once
#include <string.h>
// Dispatch/serialization fixture only, not a base64 primitive test.
struct base64_encodestate {};
inline void base64_init_encodestate(base64_encodestate*){}
inline int base64_encode_block(const char*,int bytes,char*out,base64_encodestate*){const int n=(bytes+2)/3*4;memset(out,'A',n);return n;}
inline int base64_encode_blockend(char*,base64_encodestate*){return 0;}
