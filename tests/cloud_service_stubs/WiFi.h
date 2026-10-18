#pragma once
#include "Arduino.h"
class Client {
 public:
  virtual ~Client()=default;
  bool live=false;
  unsigned stops=0;
  uint32_t connectionTimeoutMs=0;
  void setConnectionTimeout(uint32_t milliseconds){connectionTimeoutMs=milliseconds;}
  virtual void stop(){live=false;++stops;}
  bool connected()const{return live;}
};
class WiFiClient:public Client {};
