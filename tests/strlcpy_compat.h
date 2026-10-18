#pragma once
#include <stddef.h>
#include <string.h>
// Ubuntu24's fortified glibc provides strlcpy. Redefining it as an Arduino
// inline stub conflicts with its gnu_inline attribute; use libc where present.
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 38)
#define PAKAR_FIXTURE_SYSTEM_STRLCPY 1
#endif
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define PAKAR_FIXTURE_SYSTEM_STRLCPY 1
#endif
#ifndef PAKAR_FIXTURE_SYSTEM_STRLCPY
inline size_t strlcpy(char *out, const char *value, size_t capacity) {
  const size_t length = strlen(value);
  if (capacity) {
    const size_t count = length < capacity - 1 ? length : capacity - 1;
    memcpy(out, value, count); out[count] = 0;
  }
  return length;
}
#endif
