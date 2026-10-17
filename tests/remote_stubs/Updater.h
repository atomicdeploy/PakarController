#pragma once
#include "ESP8266WiFi.h"
constexpr int U_FLASH=0;
class TestUpdater{
 public:std::vector<uint8_t>staged;bool failCommit=false;unsigned commits=0;
 void runAsync(bool){}bool begin(uint32_t,int){staged.clear();return true;}
 size_t write(const uint8_t*p,size_t n){staged.insert(staged.end(),p,p+n);return n;}
 bool end(bool commit){if(!commit||failCommit)return false;ESP.flash=staged;++commits;return true;}
 String getErrorString()const{return failCommit?String("fixture OTA commit failed"):String();}
};
extern TestUpdater Update;
