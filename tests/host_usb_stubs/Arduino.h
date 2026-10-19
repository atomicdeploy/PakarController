#pragma once
#include "../serial_stubs/Arduino.h"
#include <vector>
inline std::vector<int> hostInitSteps;
inline bool hostNativeConnected=false;
class FixtureHostPort:public HardwareSerial {
 public:
  int peek(){return received.empty()?-1:received.front();}
  void flush(){}
  int availableForWrite(){return 256;}
  size_t setRxBufferSize(size_t n){hostInitSteps.push_back(1);return n;}
  void setTxTimeoutMs(uint32_t n){hostInitSteps.push_back(n==0?2:-2);}
  void begin(unsigned long n){hostInitSteps.push_back(3);HardwareSerial::begin(n);}
  operator bool()const{return hostNativeConnected;}
};
inline FixtureHostPort Serial0,Serial;
