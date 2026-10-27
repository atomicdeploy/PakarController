#pragma once
#include <stdint.h>
namespace BoardCore {
struct KeyPresentation {
  enum Motion : uint8_t { Solid, Pulse, Ring, SharedChase };
  uint16_t flashOnMs=120, flashOffMs=100, fadeMs=180, pulseMs=700, chaseMs=1800;
  uint8_t flashes=4;
  uint16_t lastFlashOnMs=400; // Zero inherits flashOnMs for saved configurations.
  struct Group {
    bool overrideColors=false;
    uint32_t win=0x00ff00,lose=0xff0000;
    uint8_t winMotion=Pulse,loseMotion=Solid;
  } groups[18];
  KeyPresentation(){
    for(uint8_t i=0;i<18;++i)
      groups[i]=Group{false,0x00ff00,0xff0000,
        uint8_t(i%2?Pulse:Ring),uint8_t(i%2?SharedChase:Ring)};
  }
  // The common state editor represents the illumination role. Indicator
  // motion and per-box deviations stay independent when that editor saves.
  void inheritIlluminationMotion(uint8_t win,uint8_t lose,bool updateWin,bool updateLose){
    const uint8_t previousWin=groups[0].winMotion,previousLose=groups[0].loseMotion;
    for(uint8_t i=0;i<18;i+=2){
      if(updateWin&&groups[i].winMotion==previousWin)groups[i].winMotion=win;
      if(updateLose&&groups[i].loseMotion==previousLose)groups[i].loseMotion=lose;
    }
  }
  uint32_t lastFlashStart()const{return flashes?uint32_t(flashes-1)*(flashOnMs+flashOffMs):0;}
  uint16_t finalOnMs()const{return lastFlashOnMs?lastFlashOnMs:flashOnMs;}
  uint32_t flashDuration()const{return flashes?lastFlashStart()+finalOnMs()+flashOffMs:0;}
  uint8_t flashIndex(uint32_t elapsed)const{return elapsed>=lastFlashStart()&&flashes?flashes-1:elapsed/(flashOnMs+flashOffMs);}
  uint16_t flashPhase(uint32_t elapsed)const{return elapsed>=lastFlashStart()?elapsed-lastFlashStart():elapsed%(flashOnMs+flashOffMs);}
  uint16_t onMs(uint32_t elapsed)const{return elapsed>=lastFlashStart()?finalOnMs():flashOnMs;}
  bool flashLit(uint32_t elapsed)const{return elapsed<flashDuration()&&flashPhase(elapsed)<onMs(elapsed);}
};
}
