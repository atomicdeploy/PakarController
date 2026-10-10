#pragma once
#include "Arduino.h"
#include <array>
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings {SPISettings(unsigned,int,int){}};
struct FakeSPI {
  std::array<uint8_t,32768> flash{},pending{};
  std::array<uint8_t,1024> eeprom{};
  uint8_t lock=0xFF;bool eesave=false,badSignature=false,badFlash=false,badEeprom=false;
  unsigned erased=0,eepromWrites=0,flashPages=0;uint8_t c[4]={},at=0;
  void begin(){}
  void end(){}
  void beginTransaction(SPISettings){at=0;}
  void endTransaction(){}
  uint8_t transfer(uint8_t byte){
    c[at++]=byte;
    if(at==3 && c[0]==0xAC && c[1]==0x53)return 0x53;
    if(at<4)return 0;
    at=0;
    const unsigned address=(static_cast<unsigned>(c[1])<<8)|c[2];
    switch(c[0]){
      case 0x30:return c[2]==0?0x1E:c[2]==1?0x95:badSignature?0:0x0F;
      case 0x58:return c[1]==0?lock:eesave?0xD6:0xDE;
      case 0x50:return c[1]==0?0xFF:0xFD;
      case 0x20:case 0x28: {
        unsigned i=address*2+(c[0]==0x28);
        return flash[i]^(badFlash&&erased&&i==0?1:0);
      }
      case 0xA0:return eeprom[address];
      case 0xAC:if(c[1]==0x80){++erased;flash.fill(0xFF);if(!eesave)eeprom.fill(0xFF);}return 0;
      case 0xC0:++eepromWrites;if(!badEeprom)eeprom[address]=c[3];return 0;
      case 0xF0:return 0;
      case 0x40:case 0x48:pending[address*2+(c[0]==0x48)]=c[3];return 0;
      case 0x4C:{
        ++flashPages;const unsigned base=address*2;
        for(unsigned i=0;i<128;++i)flash[base+i]=pending[base+i];
        return 0;
      }
    }return 0;
  }
};
inline FakeSPI SPI;
