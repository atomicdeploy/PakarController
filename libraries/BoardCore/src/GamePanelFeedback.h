#pragma once
#include <stdint.h>
#include <string.h>
#include "KeyMiniGame.h"

namespace BoardCore {
// PuzzleBoard.ino MODE_RESET::waitCounter (100 ms): bottom left to right,
// up the right edge, top right to left, down the left edge. Translate the
// donor's active-low shift-register masks to TM1637 active-high A..G bits.
inline void puzzlePanelSegments(uint32_t elapsed, uint8_t out[4]) {
  constexpr uint8_t digit[]={0,1,2,3,3,3,3,2,1,0,0,0};
  constexpr uint8_t segment[]={8,8,8,8,4,2,1,1,1,1,32,16};
  const uint8_t step=(elapsed/100U)%12U;
  memset(out,0,4);out[digit[step]]=segment[step];
}

// A fresh selection, never a resumed winner/countdown. Runtime mode changes
// remain explicit and are saved separately from this RAM-only boot choice.
inline KeyMiniGame::Config bootGameConfig(const KeyMiniGame::Config &saved, bool last) {
  using G=KeyMiniGame;
  auto first=G::PressToWin;
  if(!G::available(first))for(uint8_t i=0;i<G::ModeCount;++i)
    if(G::available(G::Mode(i))){first=G::Mode(i);break;}
  if((last||saved.mode==first)&&saved.valid())return saved;
  auto next=G::Config::defaults(first);next.enabledMask=saved.enabledMask;
  memcpy(next.outputs,saved.outputs,sizeof(next.outputs));return next;
}

// Edge-driven feedback. New rounds, pause/resume, stalls and disabled audio
// only establish a baseline: never burst/replay missed ticks or a late minute.
class GameTimerFeedback {
 public:
  enum Cue : uint8_t { None, SecondEven, SecondOdd, Minute };
  Cue update(bool active,uint32_t elapsed,uint32_t remaining,bool enabled,bool alternate) {
    if(!active){armed_=false;return None;}
    const uint32_t second=elapsed/1000U;
    if(!armed_){armed_=true;second_=second;elapsed_=elapsed;return None;}
    const uint32_t previous=second_,delta=elapsed-elapsed_;
    second_=second;elapsed_=elapsed;
    if(!enabled||!remaining||second==previous||second!=previous+1U||delta>250U||elapsed%1000U>150U)return None;
    if(second&&second%60U==0)return Minute;
    return alternate&&(second&1U)?SecondOdd:SecondEven;
  }
 private:
  uint32_t second_=0,elapsed_=0;
  bool armed_=false;
};
} // namespace BoardCore
