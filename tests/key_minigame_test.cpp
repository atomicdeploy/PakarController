#include <cassert>
#include <cstdio>
#include <KeyMiniGame.h>
using Game=BoardCore::KeyMiniGame;
using Config=Game::Config;
static void tap(Game &g,uint8_t key,uint32_t now){assert(g.input(key,true,now));assert(g.input(key,false,now+1));}
static Config config(Game::Mode mode,uint16_t mask=15){auto c=Config::defaults(mode);c.enabledMask=mask;c.introEnabled=false;return c;}
static void configure(Game &g,Config c,uint32_t now=0){assert(g.configure(c,now));assert(g.phase()==Game::Ready);}
static void basics(){
  const auto simon=Config::defaults(Game::Simon);assert(simon.rounds==1&&simon.sequenceLength==8&&simon.valid());
  Game g;auto c=config(Game::PressToWin);configure(g,c);
  assert(c.limitMs==0);g.tick(600000);assert(g.phase()==Game::Ready);
  tap(g,2,600000);assert(g.view().winner==2&&g.phase()==Game::Success);
  assert(!g.input(1,true,600010));g.reset(600020);assert(g.view().winner==0);
  assert(!g.input(9,true,600030));
  c.outputs[0]={Game::KeyWinner,2};c.outputs[1]={Game::KeyLost,1};c.outputs[2]={Game::RoundSuccess,0};
  configure(g,c);tap(g,2,10);assert(g.outputMask()==7);g.reset(20);assert(g.outputMask()==0);
  tap(g,2,21);assert(g.outputMask()==7);g.stop(23);assert(g.outputMask()==0&&g.view().winner==0);
  c.outputs[0]={Game::KeyHeld,0};assert(!g.configure(c,30));assert(g.config().outputs[0].key==2);
  assert(!Game::available(Game::Mode(255)));Game::Mode mode;assert(!Game::parse("made-up",mode));
}
static void timers(){
  Game g;auto c=config(Game::PressToWin);c.limitMs=1000;c.warningMs=700;c.urgentMs=200;
  configure(g,c,0xfffffff0);assert(g.start(0xfffffff0,7));
  g.tick(0x180);assert(g.view().elapsedMs==400&&g.view().warning==1);
  assert(g.pause(0x180));g.tick(100000);assert(g.view().elapsedMs==400);
  assert(g.resume(100000));g.tick(100400);assert(g.view().warning==2);
  g.tick(100600);assert(g.phase()==Game::Timeout);assert(!g.input(1,true,100601));
  g.reset(0);c.urgentMs=800;c.warningMs=700;assert(!g.configure(c,0));
}
static void reaction(){
  Game g;auto c=config(Game::Illuminated);c.rounds=2;c.fixedTarget=2;
  configure(g,c);assert(g.start(100,1));assert(g.view().target==2);
  tap(g,2,190);assert(g.phase()==Game::Feedback&&g.view().lastReactionMs==90);
  assert(!g.input(3,true,200));g.tick(430);assert(g.phase()==Game::Input&&g.view().round==2);
  tap(g,2,500);g.tick(740);assert(g.phase()==Game::Success&&g.view().bestReactionMs==70);
  assert(c.windowMs==10000);configure(g,c);g.start(100,1);g.tick(10099);assert(g.phase()==Game::Input);
  g.tick(10100);assert(g.phase()==Game::Timeout);
  c.windowMs=20000;configure(g,c);g.start(0,1);g.tick(19999);assert(g.phase()==Game::Input);g.tick(20000);assert(g.phase()==Game::Timeout);
  c=config(Game::ReactionChase);configure(g,c);g.start(0,1);const auto first=g.view().target;
  g.tick(c.windowMs);assert(g.phase()==Game::Input&&g.view().target!=first);
}
static void colorMatch(){
  const uint16_t masks[]={3,15,63,511,0x125};
  for(uint16_t mask:masks)for(uint8_t seed=1;seed<=32;++seed){
    Game trial;auto rules=config(Game::ColorMatch,mask);configure(trial,rules);trial.start(0,seed);
    uint32_t at=1;Game::Style seen[18];
    for(uint8_t key=1;key<=9;++key)if(mask&Game::keyBit(key))for(uint8_t tries=0;tries<rules.paletteSize&&trial.phase()==Game::Input;++tries){
      trial.styles(seen);if(seen[(key-1)*2].rgb==seen[(key-1)*2+1].rgb)break;
      tap(trial,key,at);at+=2;
    }
    assert(trial.phase()==Game::Success&&trial.matchingMask()==mask);
  }
  Game g;auto c=config(Game::ColorMatch);configure(g,c);g.start(0,17);
  Game::Style s[18];g.styles(s);assert(g.phase()==Game::Input);
  uint32_t now=10;
  for(uint8_t k=1;k<=4&&g.phase()==Game::Input;++k){
    g.styles(s);for(uint8_t tries=0;s[(k-1)*2].rgb!=s[(k-1)*2+1].rgb&&tries<9;++tries){tap(g,k,now);now+=2;g.styles(s);}
  }
  assert(g.phase()==Game::Success);
  assert(g.takeNote()!=0);assert(g.takeNote()==0);
  configure(g,c);g.start(0,4);g.styles(s);const uint32_t before=s[0].rgb;
  assert(g.input(1,true,20));g.tick(3019);g.styles(s);assert(s[0].rgb!=before);
  g.tick(3020);g.styles(s);const auto after=s[0].rgb;
  bool correct=false;for(uint8_t i=0;i<c.paletteSize;++i)if(Game::color(i)==before)
    correct=after==Game::color((i+c.paletteSize-1)%c.paletteSize);
  assert(correct);g.tick(3050);g.styles(s);assert(s[0].rgb==after);assert(g.input(1,false,3051));
}
static void simon(){
  Game g;auto c=config(Game::Simon);c.sequenceLength=2;c.rounds=2;configure(g,c);g.start(0,22);
  uint32_t now=0;
  for(uint8_t round=0;round<2;++round){
    uint8_t sequence[32]{},size=0;
    while(g.phase()==Game::Show){assert(!g.input(9,true,now));sequence[size++]=g.view().target;
      assert(g.takeNote()!=0);assert(g.takeNote()==0);
      now+=c.showMs;g.tick(now);assert(g.view().target==0);
      now+=c.gapMs;g.tick(now);
    }
    assert(size==2+round&&g.phase()==Game::Input);
    for(uint8_t i=0;i<size;++i){now+=10;tap(g,sequence[i],now);
      assert(g.phase()==Game::Feedback);Game::Style s[18];g.styles(s);
      assert(s[(sequence[i]-1)*2].rgb==Game::color(sequence[i]-1));
      assert(g.takeNote()==Game::frequency(sequence[i]-1));
      g.tick(now+c.feedbackMs-1);assert(g.phase()==Game::Feedback);
      now+=c.feedbackMs;g.tick(now);
      if(i+1<size)assert(g.phase()==Game::Input);
    }
  }
  assert(g.phase()==Game::Success);
}
static void memory(){
  Game g;auto c=config(Game::MemoryPairs,511);configure(g,c);assert(g.view().participatingMask==255);
  assert(g.config().enabledMask==511);g.start(0,18);assert(!g.input(9,true,1));
  // Reveal every pair through actual inputs, using only visible colors.
  uint32_t now=10;Game::Style s[18];uint32_t colors[8]{};
  for(uint8_t k=1;k<=8;++k){tap(g,k,now);g.styles(s);colors[k-1]=s[(k-1)*2].rgb;
    if(k%2==0){now+=c.mismatchMs;g.tick(now);}now+=2;}
  // Start same seed to verify remembered pairs, including non-adjacent pairs.
  g.stop(now);assert(g.start(now,18));
  bool used[8]{};
  for(uint8_t a=0;a<8;++a)if(!used[a])for(uint8_t b=a+1;b<8;++b)if(!used[b]&&colors[a]==colors[b]){
    now+=2;tap(g,a+1,now);now+=2;tap(g,b+1,now);used[a]=used[b]=true;
    now+=c.mismatchMs;g.tick(now);break;
  }
  assert(g.phase()==Game::Success);
  c.enabledMask=1;assert(!g.configure(c,now));
}
static void clearOrderSwitch(){
  Game g;auto c=config(Game::ClearAll,0x105);configure(g,c);g.start(0,9);
  tap(g,1,10);tap(g,3,20);tap(g,9,30);assert(g.phase()==Game::Success&&g.view().progress==3);
  assert(g.takeNote()==Game::frequency(8));assert(g.takeNote()==0);
  c=config(Game::ColorOrder);configure(g,c);g.start(0,9);Game::Style s[18];g.styles(s);
  uint32_t now=10;for(uint8_t i=0;i<4;++i)for(uint8_t k=1;k<=4;++k)
    if(s[(k-1)*2].rgb==Game::color(i)){tap(g,k,now);now+=2;break;}
  assert(g.phase()==Game::Success);
  c=config(Game::ColorOrder,511);configure(g,c);c.paletteSize=7;assert(!g.configure(c,0));
  c=config(Game::SwitchPattern);configure(g,c);g.start(0,8);g.styles(s);
  now=10;for(uint8_t k=1;k<=4;++k)if(s[(k-1)*2+1].rgb){tap(g,k,now);now+=2;}
  assert(g.phase()==Game::Success);
}
static void toggle(){
  Game g;auto c=config(Game::Toggle);assert(c.limitMs==0);c.outputs[0]={Game::KeyLatched,1};
  configure(g,c);assert(g.input(1,true,10));assert(!g.input(1,true,11));assert(g.outputMask()==1);
  assert(g.input(1,false,12));tap(g,1,13);assert(g.outputMask()==0);
  c.toggleKind=Game::Momentary;configure(g,c);assert(g.input(1,true,10));assert(g.outputMask()==1);
  g.pause(11);assert(g.input(1,false,12));assert(g.outputMask()==0);g.resume(20);
  c.toggleKind=Game::Latch;configure(g,c);tap(g,1,10);tap(g,1,20);assert(g.outputMask()==1);
  g.stop(30);assert(g.outputMask()==0);
}
static void introductions(){
  const Game::Mode modes[]={Game::ColorMatch,Game::MemoryPairs};
  for(const auto mode:modes){
    Game g;auto c=Config::defaults(mode);c.enabledMask=0x125; // Non-adjacent boxes 1,3,6,9.
    assert(c.introEnabled&&c.colorArpeggio);configure(g,c);assert(g.start(0,17));
    assert(g.phase()==Game::Show&&g.view().target==1&&g.takeNote()!=0);
    Game::Style s[18];g.styles(s);assert(s[0].rgb==0&&s[4].rgb==0);
    g.tick(500);g.styles(s);assert(s[0].rgb!=0&&s[1].rgb!=0&&s[4].rgb==0);
    assert(!g.input(1,true,501));assert(g.view().elapsedMs==0);
    const uint8_t keys[]={1,3,6,9};
    for(uint8_t i=0;i<4;++i){g.tick((i+1)*1000);g.styles(s);
      if(i<3){assert(g.view().target==keys[i+1]);assert(g.takeNote()!=0);}
      assert(s[(keys[i]-1)*2+1].rgb!=0);
    }
    const uint32_t now=4000+(mode==Game::MemoryPairs?c.previewMs:c.gapMs);
    g.tick(now);assert(g.phase()==Game::Input&&g.view().elapsedMs==0);
    g.styles(s);
    if(mode==Game::MemoryPairs)for(const auto &style:s)assert(style.rgb==0);
    else{
      uint32_t at=now+1;
      for(uint8_t k:keys)for(uint8_t tries=0;tries<9&&g.phase()==Game::Input;++tries){
        g.styles(s);if(s[(k-1)*2].rgb==s[(k-1)*2+1].rgb)break;
        tap(g,k,at);at+=2;
      }
      assert(g.phase()==Game::Success&&g.view().progress==4&&g.matchingMask()==c.enabledMask);
    }
  }
}
static void wrongPairFeedback(){
  Game g;auto c=config(Game::MemoryPairs);configure(g,c);assert(g.start(0,18));
  Game::Style s[18];uint32_t colors[4]{};uint32_t now=1;
  for(uint8_t k=1;k<=4;++k){tap(g,k,now);g.styles(s);colors[k-1]=s[(k-1)*2].rgb;
    if(k%2==0){now+=c.mismatchMs;g.tick(now);}now+=2;}
  g.stop(now);assert(g.start(now,18));uint8_t a=1,b=2;
  while(colors[a-1]==colors[b-1])++b;
  tap(g,a,++now);tap(g,b,++now);
  assert(g.phase()==Game::Feedback&&!g.feedbackCorrect());g.styles(s);
  assert(s[(a-1)*2].effect==Game::Presentation::Blink&&s[(a-1)*2].periodMs==400);
  const auto epoch=g.presentationAt();assert(epoch==now);
  g.tick(now+c.mismatchMs-1);assert(g.phase()==Game::Feedback);
  g.tick(now+c.mismatchMs);assert(g.phase()==Game::Input);g.styles(s);
  assert(s[(a-1)*2].rgb==0&&s[(b-1)*2].rgb==0);
}
int main(){
#ifdef TEST_EXCLUDED_MODES
  static_assert(Game::compiledMask()==1,"Only press-to-win should be compiled");
  Game g;auto c=config(Game::Toggle);assert(!g.configure(c,0));
#else
  static_assert(Game::compiledMask()==1023,"All games included by default");
  basics();timers();reaction();colorMatch();simon();memory();clearOrderSwitch();toggle();introductions();wrongPairFeedback();
  // Deterministic bounded soak with arbitrary input/timer sequences.
  for(uint8_t m=0;m<Game::ModeCount;++m){Game g;auto c=config(Game::Mode(m),511);c.paletteSize=9;
    configure(g,c);g.start(0,42);for(uint32_t i=1;i<15000;++i){g.tick(i*17);g.input(i%9+1,i%2,i*17);
      Game::Style s[18];g.styles(s);for(auto &v:s)assert(v.valid());
      assert(g.view().participatingMask<=511);if(i%1000==0){g.stop(i*17);g.start(i*17,i);}}
  }
#endif
  std::puts("key minigame core assertions passed (no hardware acceptance)");
}
