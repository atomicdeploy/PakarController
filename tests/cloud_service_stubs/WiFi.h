#pragma once
#include "Arduino.h"
class Client {
 public:
  virtual ~Client()=default;
  bool live=false;
  unsigned stops=0;
  virtual void stop(){live=false;++stops;}
  bool connected()const{return live;}
};
class WiFiClient:public Client {};
