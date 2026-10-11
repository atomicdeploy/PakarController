#pragma once
#include <vector>
#include <cassert>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/sha.h>
#endif
// Actual SHA-256 through host crypto. Filesystem, TLS and updater are doubles;
// this does not test BearSSL's implementation, electrical timing or flash wear.
struct br_sha256_context{std::vector<uint8_t>bytes;};
inline void br_sha256_init(br_sha256_context*c){c->bytes.clear();}
inline void br_sha256_update(br_sha256_context*c,const void*p,size_t n){auto*b=static_cast<const uint8_t*>(p);c->bytes.insert(c->bytes.end(),b,b+n);}
inline void br_sha256_out(const br_sha256_context*c,void*out){
#ifdef _WIN32
 BCRYPT_ALG_HANDLE h=nullptr;assert(BCryptOpenAlgorithmProvider(&h,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0);
 ULONG length=0,read=0;
 assert(BCryptGetProperty(h,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&read,0)==0);
 std::vector<uint8_t>object(length);BCRYPT_HASH_HANDLE hash=nullptr;
 assert(BCryptCreateHash(h,&hash,object.data(),length,nullptr,0,0)==0);
 assert(BCryptHashData(hash,const_cast<uint8_t*>(c->bytes.data()),static_cast<ULONG>(c->bytes.size()),0)==0);
 assert(BCryptFinishHash(hash,static_cast<uint8_t*>(out),32,0)==0);
 assert(BCryptDestroyHash(hash)==0);
 assert(BCryptCloseAlgorithmProvider(h,0)==0);
#else
 SHA256(c->bytes.data(),c->bytes.size(),static_cast<unsigned char*>(out));
#endif
}
