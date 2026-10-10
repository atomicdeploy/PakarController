#pragma once
#include <algorithm>
#include <string>
#include <stdint.h>
#include <cstring>
#include <cstdarg>
#include <cstdio>
using __FlashStringHelper=char;
#define F(x) x
#define OUTPUT_OPEN_DRAIN 1
#define INPUT 0
#define HIGH 1
#define LOW 0
#define MOSI 13
#define SCK 14
using std::min;
class String:public std::string {
 public:using std::string::string;String()=default;
 String(const std::string &s):std::string(s){}
};
inline void pinMode(uint8_t,int){}
inline void digitalWrite(uint8_t,int){}
inline void delay(unsigned){}
inline void yield(){}
