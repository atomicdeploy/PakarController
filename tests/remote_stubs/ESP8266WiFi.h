#pragma once
#include "Arduino.h"
#include <vector>
class WiFiClient{
 public:virtual ~WiFiClient()=default;std::vector<uint8_t>bytes;size_t at=0;
 int available()const{return static_cast<int>(min<size_t>(1024,bytes.size()-at));}
 int read(uint8_t*out,size_t n){n=min(n,bytes.size()-at);memcpy(out,bytes.data()+at,n);at+=n;return n;}
};
class TestEsp{
 public:std::vector<uint8_t>flash;bool crc=true;unsigned restarts=0;uint32_t heap=50000;
 uint32_t getFreeHeap()const{return heap;}uint32_t getMaxFreeBlockSize()const{return heap;}
 uint8_t getFlashChipMode()const{return 2;}uint32_t getFreeSketchSpace()const{return 1048576;}
 bool checkFlashCRC()const{return crc;}
 bool flashRead(uint32_t at,uint8_t*out,size_t n){if(at+n>flash.size())return false;memcpy(out,flash.data()+at,n);return true;}
 void restart(){++restarts;}
};
extern TestEsp ESP;
