#pragma once
#include "WiFi.h"
namespace BearSSL {
class X509List {public:explicit X509List(const char*){}};
class WiFiClientSecure:public WiFiClient {
 public:void setTrustAnchors(X509List*){}void setBufferSizes(unsigned,unsigned){}
};
}
