#pragma once
#include "ESP8266WiFi.h"
namespace BearSSL{class X509List{};class WiFiClientSecure:public WiFiClient{
 public:void setTrustAnchors(X509List*){}void setBufferSizes(int,int){}void setTimeout(int){}
};}
