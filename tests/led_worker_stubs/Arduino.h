#pragma once
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#define F(value) value
inline uint32_t millis(){
  static const auto start=std::chrono::steady_clock::now();
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());
}
class String:public std::string{
 public:
  using std::string::string;
  using std::string::operator+=;
  String& operator+=(unsigned value){append(std::to_string(value));return *this;}
  String& operator+=(int value){append(std::to_string(value));return *this;}
};
