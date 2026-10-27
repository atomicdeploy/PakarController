#pragma once
#include <stdint.h>
#include <pccontroller/ProtocolContract.h>

namespace BoardCore {
// PCController ControllerEvents::rfReceived wire layout. The AVR learned-id
// byte is deliberately not an authorization: ESP owns learning and actions.
struct RadioReception {
  uint32_t code=0;
  uint16_t pulseMicros=0;
  uint8_t bits=0, protocol=0;
  bool decode(const uint8_t *p,uint8_t length) {
    if(!p || length!=11 || p[0]!=ControllerProtocol::Event || p[1]!=8)return false;
    const uint32_t value=uint32_t(p[2]) | uint32_t(p[3])<<8 |
      uint32_t(p[4])<<16 | uint32_t(p[5])<<24;
    if(!value || !p[6] || p[6]>32 || !p[7])return false;
    code=value;bits=p[6];protocol=p[7];pulseMicros=uint16_t(p[8]) | uint16_t(p[9])<<8;
    return true;
  }
};
}
