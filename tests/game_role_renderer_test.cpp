#include "LedScene.h"
#include "../firmware/esp32s3/src/LedWire.h"
#include <cassert>
#include <cstdio>
int main() {
  using Presentation=BoardCore::KeyPresentation;
  Presentation defaults;
  for(uint8_t box=0;box<9;++box){
    assert(defaults.groups[box*2].winMotion==Presentation::Ring);
    assert(defaults.groups[box*2].loseMotion==Presentation::Ring);
    assert(defaults.groups[box*2+1].winMotion==Presentation::Pulse);
    assert(defaults.groups[box*2+1].loseMotion==Presentation::SharedChase);
  }
  defaults.groups[8].winMotion=Presentation::Solid; // Explicit box5 override.
  defaults.groups[10].loseMotion=Presentation::Pulse;
  defaults.inheritIlluminationMotion(Presentation::Ring,Presentation::Ring,true,true);
  assert(defaults.groups[8].winMotion==Presentation::Solid);
  assert(defaults.groups[9].winMotion==Presentation::Pulse);
  defaults.inheritIlluminationMotion(Presentation::Pulse,Presentation::Solid,true,true);
  assert(defaults.groups[0].winMotion==Presentation::Pulse);
  assert(defaults.groups[8].winMotion==Presentation::Solid);
  assert(defaults.groups[10].loseMotion==Presentation::Pulse);
  assert(defaults.groups[9].loseMotion==Presentation::SharedChase);
  BoardCore::KeyPresentation timing;
  timing.flashes=4;timing.flashOnMs=120;timing.flashOffMs=100;timing.lastFlashOnMs=400;
  assert(timing.flashDuration()==1160);
  for(uint32_t start:{0U,220U,440U}){
    assert(timing.flashLit(start));assert(timing.flashLit(start+119));
    assert(!timing.flashLit(start+120));
  }
  assert(timing.flashLit(660));assert(timing.flashLit(1059));
  assert(!timing.flashLit(1060));assert(!timing.flashLit(1160));
  assert(timing.flashIndex(900)==3 && timing.flashPhase(900)==240);
  timing.flashes=0;assert(!timing.flashLit(0)&&timing.flashDuration()==0);
  const CRGB sample(17,43,91);uint8_t wire[3]{};
  PakarS3::encodeWireBytes(&sample,1,255,true,wire);
  assert(wire[0]==(PAKAR_FIRST_WINS_ENABLED?43:91));
  assert(wire[1]==(PAKAR_FIRST_WINS_ENABLED?17:43));
  assert(wire[2]==(PAKAR_FIRST_WINS_ENABLED?91:17));
  using namespace BoardCore::Protocol;
  SceneState tableScene;tableScene.mode=1;tableScene.flags=7;tableScene.cells[0]=kCellRed;
  LedGrid grid;grid.begin(false);restoreLedScene(grid,tableScene);
  assert(grid.state(1)==(PAKAR_TABLE_SELECTION?kCellRed:kCellWhite));
  assert(grid.redLock()==bool(PAKAR_TABLE_SELECTION));
  if(PAKAR_TABLE_SELECTION) {
    assert(!grid.resetWhite());grid.setRedLock(false);assert(grid.resetWhite());
    grid.showWinner(9);assert(grid.state(80)==kCellWhite); // Other game's renderer is compiled out.
  }else{
    assert(grid.resetWhite()); // A locked table scene never traps first-wins reset.
    grid.setRedLock(true);assert(!grid.redLock());
    assert(LedGrid::kPixels==216 && LedGrid::kCells==18 && LedGrid::kSectorsPerCell==12);
    for(uint8_t player=1;player<=9;++player){
      grid.showWinner(player);grid.service(pixelTestNow+=1000);
      for(unsigned pixel=0;pixel<216;++pixel)
        assert(FastLED.pixels[pixel]==(pixel/24+1==player?CRGB(0,255,0):CRGB::Red));
    }
    grid.resetWhite();grid.service(pixelTestNow+=1000);
    for(unsigned pixel=0;pixel<216;++pixel)assert(FastLED.pixels[pixel]==CRGB::White);
    assert(grid.diagnostic(204,12,0x00ff00,false));
    assert(!grid.diagnostic(210,12,0x00ff00,false));
    grid.configureKeys(3,0xffffff,0x00ff00,0xff0000);
    grid.showWinner(2);grid.service(pixelTestNow+=1000);
    for(unsigned pixel=0;pixel<216;++pixel)
      assert(FastLED.pixels[pixel]==(pixel>=48?CRGB::Black:pixel>=24?CRGB(0,255,0):CRGB::Red));
    grid.setPattern(LedGrid::kPatternRainbow);grid.service(pixelTestNow+=1000);
    for(unsigned pixel=48;pixel<216;++pixel)assert(FastLED.pixels[pixel]==CRGB::Black);
    grid.configureKeys(511,0xffffff,0x00ff00,0xff0000);grid.service(pixelTestNow+=1000);
    for(unsigned pixel=0;pixel<216;++pixel)assert(FastLED.pixels[pixel]==CRGB::White);
    grid.configureKeys(3,0xffffff,0x00ff00,0xff0000);
    BoardCore::KeyPresentation presentation;
    grid.configureKeyPresentation(presentation);
    grid.setKeyEffects(true);grid.setStaticMode(false);grid.showWinner(1);
    grid.service(pixelTestNow+=650);
    for(unsigned pixel=0;pixel<12;++pixel)assert(FastLED.pixels[pixel]==CRGB::Black);
    grid.service(pixelTestNow+=150);
    for(unsigned pixel=0;pixel<12;++pixel)assert(FastLED.pixels[pixel]==CRGB(0,255,0));
    for(unsigned pixel=12;pixel<24;++pixel)assert(FastLED.pixels[pixel]==FastLED.pixels[12]);
    grid.service(pixelTestNow+=1000);
    bool chaseVaries=false;
    for(unsigned pixel=1;pixel<12;++pixel)chaseVaries|=FastLED.pixels[pixel]!=FastLED.pixels[0];
    assert(chaseVaries);
    for(unsigned pixel=12;pixel<24;++pixel)assert(FastLED.pixels[pixel]==FastLED.pixels[12]);
    for(unsigned pixel=0;pixel<24;++pixel)assert(FastLED.pixels[pixel].r==0&&FastLED.pixels[pixel].b==0);
    for(unsigned pixel=24;pixel<48;++pixel)assert(FastLED.pixels[pixel].g==0&&FastLED.pixels[pixel].b==0);
    grid.configureKeyMotion(500,false,true);grid.service(pixelTestNow+=50);
    for(unsigned pixel=12;pixel<24;++pixel)assert(FastLED.pixels[pixel]==FastLED.pixels[12]);
    for(unsigned pixel=48;pixel<216;++pixel)assert(FastLED.pixels[pixel]==CRGB::Black);
    assert(grid.diagnostic(0,12,0x0000ff,false));grid.service(pixelTestNow+=1000);
    auto frame=grid.pixelFrameJson();auto start=frame.find("\"rgb\":\"")+7;
    for(unsigned pixel=0;pixel<216;++pixel)assert(frame.substr(start+pixel*6,6)==(pixel<12?"0000ff":"000000"));
    // The 12-pixel layer leaves the latched game intact and restores it on stop.
    assert(grid.state(1)==kCellWon&&grid.state(3)==kCellLost);
    BoardCore::PixelDiagnostic overlay;overlay.composite=BoardCore::PixelDiagnostic::Overlay;
    overlay.effect=BoardCore::PixelDiagnostic::Blink;
    assert(grid.diagnostic(24,12,0x0000ff,false,overlay));grid.service(pixelTestNow+=100);
    frame=grid.pixelFrameJson();start=frame.find("\"rgb\":\"")+7;
    for(unsigned pixel=24;pixel<36;++pixel)assert(frame.substr(start+pixel*6,6)=="0000ff");
    overlay.enabled=false;assert(grid.diagnostic(24,12,0x0000ff,false,overlay));
    assert(!grid.diagnosticActive()&&grid.state(1)==kCellWon&&grid.state(3)==kCellLost);
    // Requested preset: two 500ms flashes, loser-only fade and a shared
    // indicator chase. Settings refresh must not restart a latched round.
    grid.configureKeys(7,0xffffff,0x00ff00,0xff0000);
    BoardCore::KeyPresentation legacy;
    legacy.flashes=2;legacy.flashOnMs=500;legacy.flashOffMs=250;
    legacy.lastFlashOnMs=0;legacy.fadeMs=750;
    grid.configureKeyPresentation(legacy);
    grid.showWinner(1);
    for(unsigned elapsed=50;elapsed<=1500;elapsed+=50){
      grid.service(pixelTestNow+=50);
      if(elapsed<1500)assert((FastLED.pixels[0]==CRGB::Black)==(elapsed%750>=500));
      if(elapsed>=750)for(unsigned pixel=24;pixel<72;++pixel)
        assert(FastLED.pixels[pixel].g==0&&FastLED.pixels[pixel].b==0);
    }
    const auto before=FastLED.pixels[0];
    grid.showWinner(1,false);grid.service(pixelTestNow+=50);
    assert(FastLED.pixels[0]!=CRGB::Black);
    (void)before;
    BoardCore::KeyPresentation quick;
    assert(quick.flashes==4&&quick.flashOnMs==120&&quick.finalOnMs()==400);
    assert(quick.fadeMs==180&&quick.pulseMs==700);
    quick.groups[2].loseMotion=BoardCore::KeyPresentation::Pulse;
    grid.configureKeyPresentation(quick);
    grid.showWinner(1);
    grid.service(pixelTestNow+=90);
    // Halfway to red is not additionally dimmed by the idle motion.
    assert(FastLED.pixels[24]==CRGB(255,127,127));
    grid.service(pixelTestNow+=90);
    assert(FastLED.pixels[24]==CRGB(255,0,0));
    grid.service(pixelTestNow+=350);
    assert(FastLED.pixels[24]==CRGB(150,0,0));
    // Every winning box, including newly enabled5..9, has a moving first
    // group and a uniform pulsing second group after the winner flashes.
    grid.configureKeys(511,0xffffff,0x00ff00,0xff0000);
    Presentation allBoxes;allBoxes.flashes=0;allBoxes.fadeMs=0;
    grid.configureKeyPresentation(allBoxes);
    for(uint8_t player=1;player<=9;++player){
      grid.showWinner(player);grid.service(pixelTestNow+=250);
      const unsigned first=(player-1)*24;
      bool varied=false;
      for(unsigned i=1;i<12;++i)varied|=FastLED.pixels[first+i]!=FastLED.pixels[first];
      assert(varied);
      for(unsigned i=13;i<24;++i)assert(FastLED.pixels[first+i]==FastLED.pixels[first+12]);
    }
  }
  puts("Baked role renderer: game exclusion and saved table-lock migration passed; host doubles only.");
}
