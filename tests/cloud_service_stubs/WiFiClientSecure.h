#pragma once
#include "WiFi.h"
class WiFiClientSecure:public WiFiClient {
 public:
  inline static int errorCode=0;
  inline static String errorText;
  inline static unsigned errorReads=0;
  void setCACert(const char*){}
  void setHandshakeTimeout(unsigned){}
  int lastError(char *out,size_t size){++errorReads;if(size)snprintf(out,size,"%s",errorText.c_str());return errorCode;}
};
