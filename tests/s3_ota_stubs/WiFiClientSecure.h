#pragma once
#include "WiFiClient.h"
class WiFiClientSecure:public WiFiClient {
 public:void setCACert(const char*){}void setTimeout(int){}int lastError(char*,size_t){return 0;}
};
