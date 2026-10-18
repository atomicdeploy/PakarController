#pragma once
#include "Arduino.h"
// This service fixture never mounts/saves configuration. Keep unused storage
// fail-closed rather than importing a second incompatible Arduino String stub.
class File {
 public:
  explicit operator bool()const{return false;}
  size_t size()const{return 0;}
  size_t read(uint8_t*,size_t){return 0;}
  size_t write(const uint8_t*,size_t){return 0;}
  void close(){}
};
namespace fs {class FS {
 public:File open(const char*,const char*){return File();}
 bool rename(const char*,const char*){return false;}
};}
