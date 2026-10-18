#include "LcdPort.h"
namespace PakarS3 {
void LcdPort::begin(uint32_t now) {
#if PAKAR_S3_LCD_ENABLED
  if(started_)return;
  started_=wire_.begin(PAKAR_S3_LCD_SDA,PAKAR_S3_LCD_SCL,100000U);
  if(started_){wire_.setTimeOut(5);display_.begin(now*1000U);}
#else
  (void)now;
#endif
}
void LcdPort::service(uint32_t now) {
  if(!started_)return;
  if(hostActive_&&static_cast<int32_t>(now-until_)>=0)release();
  display_.service(micros());
}
bool LcdPort::present(const char *one,const char *two,uint32_t holdMs,uint32_t now) {
  if(!started_||!display_.available()||!holdMs||holdMs>60000U)return false;
  display_.setText(one,two);hostActive_=true;until_=now+holdMs;return true;
}
void LcdPort::home(const char *one,const char *two,uint32_t now) {
  snprintf(home_[0],sizeof(home_[0]),"%s",one?one:"");
  snprintf(home_[1],sizeof(home_[1]),"%s",two?two:"");
  if(hostActive_&&static_cast<int32_t>(now-until_)>=0)hostActive_=false;
  if(!hostActive_)display_.setText(home_[0],home_[1]);
}
bool LcdPort::probe(uint8_t address) {
#if PAKAR_S3_LCD_ENABLED
  wire_.beginTransmission(address);return wire_.endTransmission()==0;
#else
  (void)address;return false;
#endif
}
bool LcdPort::write(uint8_t address,uint8_t value) {
#if PAKAR_S3_LCD_ENABLED
  wire_.beginTransmission(address);
  if(wire_.write(value)!=1){wire_.endTransmission();return false;}
  return wire_.endTransmission()==0;
#else
  (void)address;(void)value;return false;
#endif
}
}
