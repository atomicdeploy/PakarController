#pragma once
#include <stddef.h>
class Preferences {
 public:
  bool begin(const char*,bool);void end();bool isKey(const char*);
  size_t getBytesLength(const char*);size_t getBytes(const char*,void*,size_t);
  size_t putBytes(const char*,const void*,size_t);
  size_t getString(const char*,char*,size_t);size_t putString(const char*,const char*);
};
