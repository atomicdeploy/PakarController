#pragma once
#include <stdint.h>
#include <string.h>
#include "HostPresentation.h"

// Individual inclusion flags are application flags, not alternative board roles.
// Keep stable names/IDs when an optional mode is compiled out.
#ifndef PAKAR_GAME_PRESS_TO_WIN
#define PAKAR_GAME_PRESS_TO_WIN 1
#endif
#ifndef PAKAR_GAME_ILLUMINATED
#define PAKAR_GAME_ILLUMINATED 1
#endif
#ifndef PAKAR_GAME_COLOR_MATCH
#define PAKAR_GAME_COLOR_MATCH 1
#endif
#ifndef PAKAR_GAME_SIMON
#define PAKAR_GAME_SIMON 1
#endif
#ifndef PAKAR_GAME_MEMORY_PAIRS
#define PAKAR_GAME_MEMORY_PAIRS 1
#endif
#ifndef PAKAR_GAME_CLEAR_ALL
#define PAKAR_GAME_CLEAR_ALL 1
#endif
#ifndef PAKAR_GAME_COLOR_ORDER
#define PAKAR_GAME_COLOR_ORDER 1
#endif
#ifndef PAKAR_GAME_SWITCH_PATTERN
#define PAKAR_GAME_SWITCH_PATTERN 1
#endif
#ifndef PAKAR_GAME_REACTION_CHASE
#define PAKAR_GAME_REACTION_CHASE 1
#endif
#ifndef PAKAR_GAME_TOGGLE
#define PAKAR_GAME_TOGGLE 1
#endif

namespace BoardCore {
// Pure, allocation-free game state. No GPIO, network, filesystem or wall clock.
// Adapted donor Puzzles.ino mode transitions/countdown/output predicates, with
// blocking tones, fixed puzzle codes and nested timed callbacks removed.
// The adapter merges physical/API edges and consumes notes once; rendering uses
// the SAME HostPresentation motion primitives as table/host effects.
class KeyMiniGame {
 public:
  enum Mode : uint8_t { PressToWin, Illuminated, ColorMatch, Simon, MemoryPairs,
    ClearAll, ColorOrder, SwitchPattern, ReactionChase, Toggle, ModeCount };
  enum Phase : uint8_t { Ready, Show, Input, Feedback, Success, Timeout, Failed, Paused, Stopped };
  enum ToggleKind : uint8_t { Flip, Latch, Momentary };
  enum OutputSource : uint8_t { Disabled, RoundActive, RoundSuccess, RoundFailure,
    KeyHeld, KeyLatched, KeyWinner, KeyLost };
  struct OutputRule {
    OutputSource source=Disabled;
    uint8_t key=0; // 1-based; zero only for phase rules.
    bool valid()const{return source<=KeyLost && (source<KeyHeld ? key==0 : key>=1&&key<=9);}
  };
  struct Config {
    Mode mode=PressToWin;
    uint16_t enabledMask=511;
    uint32_t limitMs=0, warningMs=10000, urgentMs=3000, penaltyMs=2000;
    uint16_t windowMs=1200, showMs=500, gapMs=200, feedbackMs=240;
    uint16_t introMs=1000, previewMs=1500, mismatchMs=1200;
    uint8_t rounds=5, sequenceLength=4, sequenceGrowth=1, paletteSize=7, fixedTarget=0;
    ToggleKind toggleKind=Flip;
    bool hard=false, introEnabled=true, colorArpeggio=true;
    OutputRule outputs[8]{};
    static Config defaults(Mode mode){
      Config c;c.mode=mode;
      if(mode!=PressToWin&&mode!=Toggle&&mode!=ClearAll)c.limitMs=60000;
      if(mode==ColorOrder)c.paletteSize=9;
      if(mode==ColorMatch){c.introMs=500;c.introEnabled=false;}
      if(mode==Illuminated)c.windowMs=10000;
      if(mode==Simon){c.rounds=3;c.sequenceLength=3;c.sequenceGrowth=2;c.showMs=300;c.gapMs=120;}
      return c;
    }
    bool valid()const{
      if(!available(mode)||!enabledMask||enabledMask>511||limitMs>86400000UL||
         warningMs>86400000UL||urgentMs>warningMs||penaltyMs>86400000UL||
         windowMs<100||windowMs>60000||showMs<100||showMs>10000||gapMs<50||gapMs>5000||
         feedbackMs<50||feedbackMs>5000||introMs<100||introMs>10000||previewMs>10000||mismatchMs<600||mismatchMs>10000||!rounds||rounds>32||!sequenceLength||sequenceLength>32||
         sequenceGrowth>31||paletteSize<2||paletteSize>9||toggleKind>Momentary||fixedTarget>9||
         (fixedTarget&&!(enabledMask&keyBit(fixedTarget))))return false;
      if(mode==MemoryPairs&&count(enabledMask)<2)return false;
      if(mode==ColorOrder&&paletteSize<count(enabledMask))return false;
      for(const auto &r:outputs)if(!r.valid())return false;
      return true;
    }
  };
  using Presentation=HostPresentation<18,216>;
  using Style=Presentation::Style;
  struct View {
    Phase phase=Ready;
    uint32_t revision=0,elapsedMs=0,remainingMs=0,lastReactionMs=0,bestReactionMs=0;
    uint16_t participatingMask=0,heldMask=0,latchedMask=0;
    uint8_t winner=0,target=0,round=0,progress=0,mistakes=0,warning=0;
    bool timed=false;
  };
  static constexpr uint16_t keyBit(uint8_t key){return key>=1&&key<=9?uint16_t(1U<<(key-1)):0;}
  static uint8_t count(uint16_t mask){uint8_t n=0;while(mask){n+=mask&1;mask>>=1;}return n;}
  static constexpr uint16_t compiledMask(){return
    (PAKAR_GAME_PRESS_TO_WIN?1U<<PressToWin:0U)|(PAKAR_GAME_ILLUMINATED?1U<<Illuminated:0U)|
    (PAKAR_GAME_COLOR_MATCH?1U<<ColorMatch:0U)|(PAKAR_GAME_SIMON?1U<<Simon:0U)|
    (PAKAR_GAME_MEMORY_PAIRS?1U<<MemoryPairs:0U)|(PAKAR_GAME_CLEAR_ALL?1U<<ClearAll:0U)|
    (PAKAR_GAME_COLOR_ORDER?1U<<ColorOrder:0U)|(PAKAR_GAME_SWITCH_PATTERN?1U<<SwitchPattern:0U)|
    (PAKAR_GAME_REACTION_CHASE?1U<<ReactionChase:0U)|(PAKAR_GAME_TOGGLE?1U<<Toggle:0U);}
  static bool available(Mode mode){return mode<ModeCount&&(compiledMask()&(1U<<mode));}
  static const char *name(Mode mode){
    switch(mode){case PressToWin:return "press-to-win";case Illuminated:return "illuminated";
      case ColorMatch:return "color-match";case Simon:return "simon";case MemoryPairs:return "memory-pairs";
      case ClearAll:return "clear-all";case ColorOrder:return "color-order";
      case SwitchPattern:return "switch-pattern";case ReactionChase:return "reaction-chase";
      case Toggle:return "toggle";default:return "unsupported";}
  }
  static bool parse(const char *text,Mode &mode){
    for(uint8_t i=0;i<ModeCount;++i)if(text&&!strcmp(text,name(Mode(i)))){mode=Mode(i);return available(mode);}
    return false;
  }
  static const char *description(Mode mode){
    switch(mode){
      case PressToWin:return "First press wins; reset for the next round";
      case Illuminated:return "Press the glowing target before its window closes";
      case ColorMatch:return "Cycle button colors to match the indicator colors";
      case Simon:return "Watch, remember and repeat the light sequence";
      case MemoryPairs:return "Reveal colors and find every matching pair";
      case ClearAll:return "Press every illuminated button to clear the board";
      case ColorOrder:return "Press the boxes in palette color order";
      case SwitchPattern:return "Toggle buttons to copy the indicator pattern";
      case ReactionChase:return "Catch the moving light inside its response window";
      case Toggle:return "Control latched or momentary outputs without a timer";
      default:return "Board-provided game";
    }
  }
  static const char *phaseName(Phase phase){
    switch(phase){case Ready:return "ready";case Show:return "show";case Input:return "input";
      case Feedback:return "feedback";case Success:return "success";case Timeout:return "timeout";
      case Failed:return "failed";case Paused:return "paused";case Stopped:return "stopped";}
    return "unknown";
  }
  // Colors and notes are paired by stable palette index, across every mode.
  static uint32_t color(uint8_t i){
    // The default seven must be unmistakable on real RGB LEDs. Cyan, blue
    // and blue-heavy violet previously occupied three of those seven slots.
    constexpr uint32_t palette[]={0xff0000,0x00ff00,0x0000ff,0xffff00,0xff00ff,0xff8000,0xffffff,0x00ffff,0xff0080};
    return palette[i%9];
  }
  static const char *colorName(uint8_t i){
    constexpr const char *names[]={"red","green","blue","yellow","magenta","orange","white","cyan","pink"};
    return names[i%9];
  }
  static uint16_t frequency(uint8_t i){
    constexpr uint16_t notes[]={523,587,659,698,784,880,988,1047,1175};return notes[i%9];
  }
  const Config &config()const{return config_;}
  // A saved older fade value remains intact, but cannot slow the color puzzle
  // to more than half a second per enabled box. Other modes retain their rule.
  uint16_t introStepMs()const{return config_.mode==ColorMatch&&config_.introMs>500?500:config_.introMs;}
  Phase phase()const{return phase_;}
  uint32_t presentationAt()const{return phaseAt_;}
  bool feedbackCorrect()const{return phase_==Feedback&&feedbackGood_;}
  bool running()const{return phase_==Show||phase_==Input||phase_==Feedback;}
  bool configure(const Config &next,uint32_t now){
    if(!next.valid())return false;
    config_=next;reset(now);return true;
  }
  // Configure/reset opens READY. Only start (or a press in the two untimed
  // utility modes) begins gameplay. UI mounting/reading has no side effects.
  void reset(uint32_t now){
    phase_=Ready;phaseAt_=lastAt_=now;elapsed_=penalty_=0;held_=latched_=matched_=0;
    winner_=target_=round_=progress_=mistakes_=first_=second_=showIndex_=0;
    lastReaction_=bestReaction_=0;note_=0;feedbackGood_=false;heldConsumed_=0;cueKey_=0;
    memset(player_,0,sizeof(player_));memset(targets_,0,sizeof(targets_));
    participating_=config_.enabledMask;
    if(config_.mode==MemoryPairs&&count(participating_)%2)
      for(uint8_t k=9;k;--k)if(participating_&keyBit(k)){participating_&=~keyBit(k);break;}
    ++revision_;
  }
  bool start(uint32_t now,uint32_t seed){
    if(!config_.valid()||running()||phase_==Paused)return false;
    reset(now);rng_=seed?seed:0x6d2b79f5U;round_=1;
    [[maybe_unused]] uint8_t keys[9]{};uint8_t n=0;
    for(uint8_t k=1;k<=9;++k)if(participating_&keyBit(k))keys[n++]=k;
    switch(config_.mode){
#if PAKAR_GAME_COLOR_MATCH
      case ColorMatch:
        for(uint8_t i=0;i<n;++i)targets_[keys[i]-1]=random(config_.paletteSize);
        // Never begin already solved, including a single-box installation.
        player_[keys[0]-1]=(targets_[keys[0]-1]+1)%config_.paletteSize;
        break;
#endif
#if PAKAR_GAME_MEMORY_PAIRS
      case MemoryPairs:
        shuffle(keys,n);
        for(uint8_t i=0;i<n;++i)targets_[keys[i]-1]=i/2;
        break;
#endif
#if PAKAR_GAME_COLOR_ORDER
      case ColorOrder:
        shuffle(keys,n);for(uint8_t i=0;i<n;++i)targets_[keys[i]-1]=i;
        break;
#endif
#if PAKAR_GAME_SWITCH_PATTERN
      case SwitchPattern:
        for(uint8_t i=0;i<n;++i)targets_[keys[i]-1]=random(2);
        targets_[keys[0]-1]=1; // Non-empty target, not trivially solved.
        break;
#endif
#if PAKAR_GAME_SIMON
      case Simon:
        for(uint8_t i=0;i<32;++i)sequence_[i]=keys[random(n)];
        enter(Show,now);target_=sequence_[0];emitNote(target_-1);return true;
#endif
      default:break;
    }
    if(config_.introEnabled&&(config_.mode==ColorMatch||config_.mode==MemoryPairs)){
      showIndex_=0;target_=1;while(!(participating_&keyBit(target_)))++target_;
      emitNote(targets_[target_-1]);enter(Show,now);return true;
    }
    enter(Input,now);
    if(config_.mode==Illuminated||config_.mode==ReactionChase)chooseTarget(now);
    return true;
  }
  void stop(uint32_t now){held_=heldConsumed_=latched_=0;winner_=0;note_=0;enter(Stopped,now);}
  bool pause(uint32_t now){
    tick(now);if(!running())return false;
    beforePause_=phase_;pausedAt_=now;phase_=Paused;note_=0;++revision_;return true;
  }
  bool resume(uint32_t now){
    if(phase_!=Paused)return false;
    const uint32_t delta=now-pausedAt_;phaseAt_+=delta;targetAt_+=delta;
    for(uint8_t k=0;k<9;++k)if(held_&keyBit(k+1))heldAt_[k]+=delta;
    lastAt_=now;phase_=beforePause_;++revision_;return true;
  }
  // API tap uses down+up; hold/press clients must send both edges. Duplicate
  // down never toggles twice. An up retires ownership even outside INPUT.
  bool input(uint8_t key,bool down,uint32_t now){
    tick(now);const uint16_t mask=keyBit(key);
    if(!(participating_&mask))return false;
    if(!down){
      if(!(held_&mask))return false;
      held_&=~mask;heldConsumed_&=~mask;
      if(config_.mode==Toggle&&config_.toggleKind==Momentary){latched_&=~mask;player_[key-1]=0;}
      ++revision_;return true;
    }
    if(held_&mask)return false;
    if(phase_==Ready&&(config_.mode==PressToWin||config_.mode==Toggle))start(now,now);
    if(phase_!=Input)return false;
    held_|=mask;heldAt_[key-1]=now;++revision_;
    switch(config_.mode){
#if PAKAR_GAME_PRESS_TO_WIN
      case PressToWin:winner_=key;latched_=mask;finish(Success,now);emitNote(key-1);break;
#endif
#if PAKAR_GAME_ILLUMINATED || PAKAR_GAME_REACTION_CHASE
      case Illuminated:case ReactionChase:
        if(key==target_){lastReaction_=now-targetAt_;if(!bestReaction_||lastReaction_<bestReaction_)bestReaction_=lastReaction_;
          feedbackGood_=true;emitNote(4);enter(Feedback,now);
        }else mistake(now);
        break;
#endif
#if PAKAR_GAME_COLOR_MATCH
      case ColorMatch:player_[key-1]=(player_[key-1]+1)%config_.paletteSize;
        emitNote(player_[key-1]);if(patternMatches())finish(Success,now);break;
#endif
#if PAKAR_GAME_SIMON
      case Simon:
        cueKey_=key;cueAt_=now;
        emitNote(key-1);
        if(sequence_[progress_]!=key){mistake(now);finish(Failed,now);}
        else{++progress_;feedbackGood_=true;enter(Feedback,now);}
        break;
#endif
#if PAKAR_GAME_MEMORY_PAIRS
      case MemoryPairs:
        if((matched_&mask)||first_==key)break;
        emitNote(targets_[key-1]);
        if(!first_)first_=key;
        else{second_=key;feedbackGood_=targets_[first_-1]==targets_[key-1];
          if(feedbackGood_)matched_|=keyBit(first_)|mask;else incrementMistakes();
          enter(Feedback,now);}
        break;
#endif
#if PAKAR_GAME_CLEAR_ALL
      case ClearAll:
        if(!(latched_&mask)){latched_|=mask;++progress_;emitNote(key-1);}
        if(latched_==participating_)finish(Success,now);
        break;
#endif
#if PAKAR_GAME_COLOR_ORDER
      case ColorOrder:
        if(latched_&mask)break;
        if(targets_[key-1]==progress_){latched_|=mask;++progress_;emitNote(targets_[key-1]);
          if(latched_==participating_)finish(Success,now);
        }else mistake(now);
        break;
#endif
#if PAKAR_GAME_SWITCH_PATTERN
      case SwitchPattern:
        if(config_.hard)player_[key-1]=1;else player_[key-1]^=1;
        emitNote(player_[key-1]?4:0);
        if(patternMatches())finish(Success,now);
        else if(config_.hard&&player_[key-1]&&!targets_[key-1])finish(Failed,now);
        break;
#endif
#if PAKAR_GAME_TOGGLE
      case Toggle:
        if(config_.toggleKind==Flip)latched_^=mask;else latched_|=mask;
        player_[key-1]=(latched_&mask)!=0;emitNote(player_[key-1]?1:0);break;
#endif
      default:return false;
    }
    return true;
  }
  bool tick(uint32_t now){
    const uint32_t previous=revision_,delta=now-lastAt_;lastAt_=now;
    if(!running())return false;
    // All intervals are below 2^31; unsigned differences survive millis wrap.
    if(!(phase_==Show&&(config_.mode==ColorMatch||config_.mode==MemoryPairs)))
      elapsed_=delta>86400000UL-elapsed_?86400000UL:elapsed_+delta;
    if(config_.limitMs&&elapsed_+penalty_>=config_.limitMs){finish(Timeout,now);return true;}
    const uint16_t feedbackDuration=config_.mode==MemoryPairs&&!feedbackGood_?config_.mismatchMs:config_.feedbackMs;
    if(phase_==Feedback&&uint32_t(now-phaseAt_)>=feedbackDuration){
      if(config_.mode==MemoryPairs){first_=second_=0;
        if(matched_==participating_)finish(Success,now);else enter(Input,now);
      }else if(config_.mode==Simon&&feedbackGood_&&progress_<sequenceSize())enter(Input,now);
      else if(config_.mode==ColorOrder||!feedbackGood_)enter(Input,now);
      else if(round_>=config_.rounds)finish(Success,now);
      else{++round_;progress_=showIndex_=0;
        if(config_.mode==Simon){enter(Show,now);target_=sequence_[0];emitNote(target_-1);}
        else{enter(Input,now);chooseTarget(now);}}
    }
#if PAKAR_GAME_SIMON
    if(phase_==Show&&config_.mode==Simon){
      const uint32_t age=now-phaseAt_;
      if(target_&&age>=config_.showMs){target_=0;++revision_;}
      if(age>=uint32_t(config_.showMs)+config_.gapMs){
        if(++showIndex_>=sequenceSize()){target_=0;enter(Input,now);}
        else{phaseAt_=now;target_=sequence_[showIndex_];emitNote(target_-1);++revision_;}
      }
    }
#endif
    if(phase_==Show&&(config_.mode==ColorMatch||config_.mode==MemoryPairs)){
      const uint16_t step=introStepMs();bool advanced=false;
      // Anchor to the previous deadline, as the shared finite tone player does.
      // Skip expired reveal steps after a stall instead of accumulating delay
      // or replaying a burst of old color notes. Work is bounded by nine boxes.
      while(target_&&uint32_t(now-phaseAt_)>=step){
        phaseAt_+=step;advanced=true;
        if(++showIndex_>=count(participating_))target_=0;
        else{
          uint8_t rank=0;
          for(uint8_t k=1;k<=9;++k)if(participating_&keyBit(k)){
            if(rank++==showIndex_){target_=k;break;}
          }
        }
      }
      if(advanced){++revision_;if(target_)emitNote(targets_[target_-1]);else note_=0;}
      const uint16_t pause=config_.mode==MemoryPairs?config_.previewMs:config_.gapMs>500?500:config_.gapMs;
      if(!target_&&uint32_t(now-phaseAt_)>=pause)enter(Input,now);
    }
    if(phase_==Input){
      if((config_.mode==Illuminated||config_.mode==ReactionChase)&&uint32_t(now-targetAt_)>=config_.windowMs){
        if(config_.mode==Illuminated)finish(Timeout,now);
        else chooseTarget(now); // Chase remains playable until overall timeout.
      }
      if(config_.mode==ColorMatch)for(uint8_t k=1;k<=9;++k)if((held_&keyBit(k))&&!(heldConsumed_&keyBit(k))&&uint32_t(now-heldAt_[k-1])>=3000){
        // Undo the immediate forward press, then move back one from the
        // pre-press color. Exactly once per hold, never per service tick.
        player_[k-1]=(player_[k-1]+2U*config_.paletteSize-2U)%config_.paletteSize;
        heldConsumed_|=keyBit(k);emitNote(player_[k-1]);++revision_;
        if(patternMatches())finish(Success,now);
      }
    }
    return revision_!=previous;
  }
  View view()const{
    View v;v.phase=phase_;v.revision=revision_;v.elapsedMs=elapsed_;v.timed=config_.limitMs!=0;
    const uint32_t used=elapsed_+penalty_;
    v.remainingMs=v.timed&&used<config_.limitMs?config_.limitMs-used:0;
    v.participatingMask=participating_;v.heldMask=held_;v.latchedMask=latched_;
    v.winner=winner_;v.target=target_;v.round=round_;v.progress=progress_;v.mistakes=mistakes_;
    if(config_.mode==ColorMatch){v.progress=0;for(uint8_t k=1;k<=9;++k)
      if((participating_&keyBit(k))&&player_[k-1]==targets_[k-1])++v.progress;}
    v.lastReactionMs=lastReaction_;v.bestReactionMs=bestReaction_;
    if(v.timed&&running())v.warning=v.remainingMs<=config_.urgentMs?2:v.remainingMs<=config_.warningMs?1:0;
    return v;
  }
  // One finite note, replaced by a newer note rather than replaying stale audio.
  uint16_t takeNote(){const uint16_t out=note_;note_=0;return out;}
  uint16_t matchingMask()const{
    uint16_t mask=0;
    for(uint8_t k=1;k<=9;++k)if((participating_&keyBit(k))&&player_[k-1]==targets_[k-1])mask|=keyBit(k);
    return mask;
  }
  bool colorMatchValues(uint8_t key,uint8_t &player,uint8_t &target)const{
    if(config_.mode!=ColorMatch||!(participating_&keyBit(key)))return false;
    player=player_[key-1];target=targets_[key-1];return true;
  }
  uint8_t outputMask()const{
    if(phase_==Stopped)return 0;
    uint8_t result=0;
    for(uint8_t i=0;i<8;++i){const auto &r=config_.outputs[i];const uint16_t key=keyBit(r.key);bool on=false;
      switch(r.source){case RoundActive:on=running();break;case RoundSuccess:on=phase_==Success;break;
        case RoundFailure:on=phase_==Failed||phase_==Timeout;break;case KeyHeld:on=(held_&key)!=0;break;
        case KeyLatched:on=(latched_&key)!=0;break;case KeyWinner:on=winner_==r.key;break;
        case KeyLost:on=winner_&&winner_!=r.key&&(participating_&key);break;default:break;}
      if(on)result|=1U<<i;
    }
    return result;
  }
  void styles(Style (&out)[18])const{
    for(auto &s:out){s.clear();s.enabled=true;}
    const auto v=view();
    uint8_t rank=0;
    for(uint8_t k=1;k<=9;++k){if(!(participating_&keyBit(k)))continue;
      const uint8_t position=rank++;
      auto &button=out[(k-1)*2],&indicator=out[(k-1)*2+1];
      if(phase_==Stopped)continue;
      if(phase_==Ready){button.rgb=indicator.rgb=0xffffff;continue;}
      if(phase_==Success){button.rgb=indicator.rgb=winner_&&winner_!=k?0xff0000:0x00ff00;
        if(config_.mode!=PressToWin&&uint32_t(lastAt_-phaseAt_)>=250&&uint32_t(lastAt_-phaseAt_)<2250)
          button.effect=indicator.effect=Presentation::Rainbow;
        continue;}
      if(phase_==Failed||phase_==Timeout){button.rgb=indicator.rgb=0xff0000;button.effect=indicator.effect=Presentation::Blink;continue;}
      if(phase_==Show&&(config_.mode==ColorMatch||config_.mode==MemoryPairs)){
        if(position<=showIndex_){
          const uint32_t age=lastAt_-phaseAt_;
          uint16_t gain=255;
          if(target_&&position==showIndex_){
            const uint16_t step=introStepMs();
            const uint32_t x=age>=step?255:uint64_t(age)*255/step;
            gain=x*x*(765-2*x)/65025; // Smoothstep, not a stepped/blinking reveal.
          }
          button.rgb=scaled(color(config_.mode==ColorMatch?player_[k-1]:targets_[k-1]),gain);
          indicator.rgb=scaled(color(targets_[k-1]),gain);
        }
        continue;
      }
      switch(config_.mode){
        case PressToWin:button.rgb=indicator.rgb=0xffffff;break;
        case Illuminated:case ReactionChase:
          if(k==target_){button.rgb=indicator.rgb=phase_==Feedback&&feedbackGood_?0x00ffff:0x00ff00;
            button.effect=Presentation::Pulse;button.periodMs=400;}
          break;
        case ColorMatch:button.rgb=color(player_[k-1]);indicator.rgb=color(targets_[k-1]);break;
        case Simon:
          if(k==target_||((phase_==Input||phase_==Feedback)&&((held_&keyBit(k))||(cueKey_==k&&uint32_t(lastAt_-cueAt_)<config_.feedbackMs))))button.rgb=indicator.rgb=color(k-1);
          break;
        case MemoryPairs:
          if((matched_&keyBit(k))||k==first_||k==second_){button.rgb=indicator.rgb=color(targets_[k-1]);
            if(phase_==Feedback&&!feedbackGood_&&(k==first_||k==second_)){button.effect=indicator.effect=Presentation::Blink;button.periodMs=indicator.periodMs=config_.mismatchMs/3;}}
          break;
        case ClearAll:button.rgb=indicator.rgb=(latched_&keyBit(k))?0:0xffffff;break;
        case ColorOrder:button.rgb=indicator.rgb=(latched_&keyBit(k))?0:color(targets_[k-1]);break;
        case SwitchPattern:button.rgb=player_[k-1]?0x00ffff:0;indicator.rgb=targets_[k-1]?0x00ffff:0;break;
        case Toggle:button.rgb=indicator.rgb=(latched_&keyBit(k))?0x00ff00:0xff0000;break;
        default:break;
      }
      if(phase_==Feedback&&!feedbackGood_&&config_.mode!=MemoryPairs){button.rgb=0xff0000;button.effect=Presentation::Blink;button.periodMs=100;}
      // Warning affects indicators only; it must not obscure the answer shown
      // by the button group. Color-match/switch reference colors stay intact.
      if(v.warning){indicator.effect=Presentation::Blink;indicator.periodMs=v.warning==2?200:600;}
    }
  }
 private:
  Config config_{};
  Phase phase_=Ready,beforePause_=Ready;
  uint32_t revision_=0,phaseAt_=0,lastAt_=0,pausedAt_=0,targetAt_=0,elapsed_=0,penalty_=0;
  uint32_t heldAt_[9]{},rng_=1,lastReaction_=0,bestReaction_=0,cueAt_=0;
  uint16_t participating_=511,held_=0,heldConsumed_=0,latched_=0,matched_=0,note_=0;
  uint8_t player_[9]{},targets_[9]{},sequence_[32]{};
  uint8_t winner_=0,target_=0,round_=0,progress_=0,mistakes_=0,first_=0,second_=0,showIndex_=0,cueKey_=0;
  bool feedbackGood_=false;
  static uint32_t scaled(uint32_t rgb,uint16_t gain){return
    (((rgb>>16)*gain/255)<<16)|((((rgb>>8)&255)*gain/255)<<8)|((rgb&255)*gain/255);}
  uint8_t random(uint8_t n){rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;return uint8_t(rng_%n);}
  void shuffle(uint8_t *items,uint8_t n){for(uint8_t i=n;i>1;--i){uint8_t j=random(i),v=items[i-1];items[i-1]=items[j];items[j]=v;}}
  uint8_t sequenceSize()const{
    const uint16_t n=config_.sequenceLength+uint16_t(round_?round_-1:0)*config_.sequenceGrowth;
    return n>32?32:n;
  }
  void emitNote(uint8_t index){note_=frequency(index);}
  void incrementMistakes(){if(mistakes_!=255)++mistakes_;}
  void enter(Phase phase,uint32_t now){phase_=phase;phaseAt_=now;++revision_;}
  void finish(Phase phase,uint32_t now){
    // Keep the completing color's finite note; failure has a red note.
    if(phase==Failed||phase==Timeout)emitNote(0);
    enter(phase,now);
  }
  bool patternMatches()const{for(uint8_t k=1;k<=9;++k)if((participating_&keyBit(k))&&player_[k-1]!=targets_[k-1])return false;return true;}
  void chooseTarget(uint32_t now){
    const uint8_t old=target_;uint8_t keys[9]{},n=0;
    for(uint8_t k=1;k<=9;++k)if(participating_&keyBit(k))keys[n++]=k;
    if(config_.fixedTarget)target_=config_.fixedTarget;
    else if(config_.mode==ReactionChase){uint8_t i=0;while(i<n&&keys[i]!=old)++i;target_=keys[i==n?0:(i+1)%n];}
    else {target_=keys[random(n)];if(n>1&&target_==old)for(uint8_t i=0;i<n;++i)if(keys[i]==old){target_=keys[(i+1)%n];break;}}
    targetAt_=now;emitNote(1);++revision_;
  }
  void mistake(uint32_t now){
    incrementMistakes();penalty_=config_.penaltyMs>86400000UL-penalty_?86400000UL:penalty_+config_.penaltyMs;
    feedbackGood_=false;emitNote(0);enter(Feedback,now);
    if(config_.limitMs&&elapsed_+penalty_>=config_.limitMs)finish(Timeout,now);
  }
};
} // namespace BoardCore
