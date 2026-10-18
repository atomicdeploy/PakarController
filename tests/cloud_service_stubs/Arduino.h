#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <type_traits>
using __FlashStringHelper=char;
#define F(value) value
#define PROGMEM
using std::min;
inline uint32_t cloudTestMillis=0;
inline uint32_t millis(){return cloudTestMillis;}
inline void yield(){}
#include "../strlcpy_compat.h"
class String:public std::string {
 public:
  using std::string::string;using std::string::operator=;
  String()=default;String(const std::string &s):std::string(s){}
  template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>
  explicit String(T value):std::string(std::to_string(value)){}
  bool endsWith(const char *s)const{const auto n=strlen(s);return size()>=n&&compare(size()-n,n,s)==0;}
  bool concat(const char *p,size_t n){append(p,n);return true;}
  bool concat(const char *p){append(p);return true;}
  void toCharArray(char *out,size_t n)const{if(n){const auto count=min(n-1,size());memcpy(out,data(),count);out[count]=0;}}
  size_t write(uint8_t c){push_back(static_cast<char>(c));return 1;}
  size_t write(const uint8_t*p,size_t n){append(reinterpret_cast<const char*>(p),n);return n;}
};
inline String operator+(const String &s,int value){return String(static_cast<const std::string&>(s)+std::to_string(value));}
