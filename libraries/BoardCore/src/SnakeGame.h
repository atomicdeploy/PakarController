#pragma once
#include <stdint.h>
namespace BoardCore {
// Optional table mini-game; independent of role, GPIO, transport and saved scene.
class SnakeGame {
 public:
  enum Phase:uint8_t { Ready,Running,Paused,Over,Won,Stopped };
  uint8_t body[80]={},length=0,food=0,direction=1;
  uint16_t score=0,interval=250;
  Phase phase=Ready;
  static constexpr uint8_t logical(uint8_t p){return 1+(p%20)/2+((p%20)%2)*10+(p/20)*20;}
  static constexpr uint8_t next(uint8_t p,uint8_t d){
    return uint8_t(((p/20+(d==2?1:d==0?3:0))%4)*20+(p%20+(d==1?1:d==3?19:0))%20);
  }
  void start(uint32_t now){length=3;body[0]=24;body[1]=23;body[2]=22;direction=1;queued_=255;score=0;food=7;phase=Running;last_=now;placeFood();}
  void stop(){phase=Stopped;queued_=255;}
  void pause(){if(phase==Running)phase=Paused;}
  void resume(uint32_t now){if(phase==Paused){phase=Running;last_=now;}}
  bool turn(uint8_t d){if(d>3||queued_!=255||(d^2U)==direction)return false;queued_=d;return true;}
  bool service(uint32_t now){
    if(phase!=Running||uint32_t(now-last_)<interval)return false;
    last_=now;if(queued_!=255){direction=queued_;queued_=255;}
    const uint8_t head=next(body[0],direction);const bool eating=head==food;
    for(uint8_t i=0;i<(eating?length:length-1);++i)if(body[i]==head){phase=Over;return true;}
    if(eating&&length<80){++length;++score;}
    for(uint8_t i=length-1;i>0;--i){body[i]=body[i-1];}
    body[0]=head;
    if(length==80){phase=Won;food=255;}else if(eating)placeFood();
    return true;
  }
 private:
  uint32_t last_=0;uint8_t queued_=255;
  void placeFood(){uint8_t candidate=(food*17U+23U)%80U;for(uint8_t n=0;n<80;++n){bool occupied=false;
    for(uint8_t i=0;i<length;++i)if(body[i]==candidate)occupied=true;
    if(!occupied){food=candidate;return;}candidate=(candidate+1)%80;}}
};
}
