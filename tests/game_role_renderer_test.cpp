#include "LedScene.h"
#include <cassert>
#include <cstdio>
int main() {
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
  }
  puts("Baked role renderer: game exclusion and saved table-lock migration passed; host doubles only.");
}
