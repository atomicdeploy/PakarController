#pragma once
#include <Arduino.h>
class FixtureUsb {
 public:
  bool productName(const char*){hostInitSteps.push_back(4);return true;}
  bool begin(){hostInitSteps.push_back(5);return true;}
};
inline FixtureUsb USB;
