#pragma once
#include <stdint.h>
class RCSwitch {
 public:
  inline static uint32_t code=0;
  inline static uint8_t bits=24,protocol=1;
  inline static uint16_t pulse=350;
  inline static unsigned sends=0,disables=0,enables=0;
  inline static int rx=-1,tx=-1,repeats=0;
  void enableTransmit(int pin){tx=pin;}
  void setRepeatTransmit(int n){repeats=n;}
  void enableReceive(int pin){rx=pin;++enables;}
  void disableReceive(){rx=-1;++disables;}
  bool available(){return code!=0;}
  uint32_t getReceivedValue(){return code;}
  uint8_t getReceivedBitlength(){return bits;}
  uint8_t getReceivedProtocol(){return protocol;}
  uint16_t getReceivedDelay(){return pulse;}
  void resetAvailable(){code=0;}
  void setProtocol(int value){protocol=value;pulse=350;}
  void setPulseLength(int value){pulse=value;}
  void send(uint32_t,uint8_t){++sends;}
};
