#pragma once
#include <Arduino.h>
#include <LcdDisplay.h>
#include "BoardPins.h"
#ifndef PAKAR_S3_LCD_ENABLED
#define PAKAR_S3_LCD_ENABLED 1
#endif
#if PAKAR_S3_LCD_ENABLED != 0 && PAKAR_S3_LCD_ENABLED != 1
#error "PAKAR_S3_LCD_ENABLED must be0 or1"
#endif
#if PAKAR_S3_LCD_ENABLED
#include <Wire.h>
#endif

namespace PakarS3 {
class LcdPort final:private BoardCore::LcdDisplay::Bus {
 public:
  LcdPort():display_(*this){}
  static constexpr bool configured(){return PAKAR_S3_LCD_ENABLED!=0;}
  void begin(uint32_t now);
  void service(uint32_t now);
  bool present(const char *one,const char *two,uint32_t holdMs,uint32_t now);
  void home(const char *one,const char *two,uint32_t now);
  void release(){hostActive_=false;display_.setText(home_[0],home_[1]);}
  void rescan(uint32_t now){if(started_)display_.rescan(now*1000U);}
  const BoardCore::LcdDisplay &display()const{return display_;}
  bool hostActive()const{return hostActive_;}
  bool started()const{return started_;}
 private:
  bool probe(uint8_t address)override;
  bool write(uint8_t address,uint8_t value)override;
#if PAKAR_S3_LCD_ENABLED
  TwoWire wire_{1}; // Independent S3 hardware I2C, never AVR A4/A5/TM1637.
#endif
  BoardCore::LcdDisplay display_;
  char home_[2][17]={{0},{0}};
  uint32_t until_=0;
  bool started_=false,hostActive_=false;
};
}
