#pragma once
#include <Arduino.h>
class USBCDC:public FixtureHostPort {
 public:
  explicit USBCDC(uint8_t){}
  bool reboot=true;
  void enableReboot(bool enabled){reboot=enabled;hostInitSteps.push_back(enabled?-1:0);}
  void begin(unsigned long n){if(reboot)hostInitSteps.push_back(-3);FixtureHostPort::begin(n);}
};
