#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#define F(value) value
using std::min;
using std::max;
constexpr uint8_t D2=4;
inline uint32_t pixelTestNow=0;
inline bool pixelTestReserve=true;
inline uint32_t millis(){return pixelTestNow;}
// Same bounded String model as access_stubs, extended only for LED serialization.
class String: public std::string {
 public:
  using std::string::string;
  using std::string::operator=;
  using std::string::operator+=;
  bool reserve(size_t n){if(!pixelTestReserve)return false;std::string::reserve(n);return true;}
  String& operator+=(uint8_t n){append(std::to_string(n));return *this;}
  String& operator+=(uint32_t n){append(std::to_string(n));return *this;}
};
