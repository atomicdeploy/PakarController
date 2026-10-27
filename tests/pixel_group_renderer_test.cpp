#include "LedGrid.h"
#include <StateMotion.h>
#include <cassert>
#include <cstdio>
using namespace BoardCore::Protocol;
int main(){
  LedGrid grid;grid.begin();
  uint8_t overrides[LedGrid::kCells]{};
  const uint8_t normal=LedGrid::kSectorsPerCell;
  overrides[0]=normal-1;overrides[1]=normal+1;overrides[2]=normal-2;overrides[3]=normal-3;
  LedGrid::GroupLayout layout;assert(layout.configure(overrides,4));
  assert(grid.configureGroups(layout));
  // Published coordinates stay attached to their captured RGB until the next
  // complete frame; live reconfiguration is explicitly pending in the interim.
  assert(grid.pixelFrameJson().find("\"group_layout_revision\":0")!=String::npos);
  assert(grid.pixelFrameJson().find("\"pending\":true")!=String::npos);
  grid.service(pixelTestNow+=1000);
  assert(grid.pixelFrameJson().find("\"group_layout_revision\":4")!=String::npos);
  assert(grid.pixelFrameJson(true).find("\"group_counts\":[")!=String::npos);
  assert(grid.pixelFrameJson(true).find("\"rgb\"")==String::npos);
  uint8_t physical[LedGrid::kCells];for(uint8_t i=0;i<LedGrid::kCells;++i)physical[i]=i+1;
  physical[0]=2;physical[1]=1;grid.setPhysicalMapping(physical,LedGrid::kCells);
  assert(grid.setCell(1,kCellRed,kEffectImmediate));grid.service(pixelTestNow+=1000);
  for(uint16_t i=0;i<LedGrid::kPixels;++i)assert(FastLED.pixels[i]==
    (i>=layout.used()?CRGB::Black:i>=layout.start(2)&&i<layout.start(2)+layout.count(2)?CRGB::Red:CRGB::White));
  assert(grid.groupLayout().count(1)==normal-1&&grid.groupLayout().count(2)==normal+1);
  assert(!grid.diagnostic(0,normal,0x00ff00,false));
  assert(grid.diagnostic(layout.start(2),layout.count(2),0x00ff00,false));
  assert(!grid.canConfigureGroups()&&!grid.configureGroups(layout));
  auto frame=grid.pixelFrameJson();auto rgbAt=frame.find("\"rgb\":\"")+7;
  for(uint16_t i=0;i<LedGrid::kPixels;++i)assert(frame.substr(rgbAt+i*6,6)==
    (i>=layout.start(2)&&i<layout.start(2)+layout.count(2)?"00ff00":"000000"));
  BoardCore::PixelDiagnostic stop;stop.enabled=false;
  assert(grid.diagnostic(layout.start(2),layout.count(2),0x00ff00,false,stop));
  assert(grid.canConfigureGroups());
  assert(!grid.diagnostic(layout.used(),1,0xffffff,false));
  BoardCore::CellBatch selection;selection.add(1);
  assert(grid.preview(selection,0x0000ff,500));assert(!grid.configureGroups(layout));
  grid.service(pixelTestNow+=100);frame=grid.pixelFrameJson();rgbAt=frame.find("\"rgb\":\"")+7;
  assert(frame.substr(rgbAt+layout.start(2)*6,6)=="0000ff");
  grid.cancelPreview();grid.setStaticMode(false);
  assert(grid.setCell(1,kCellWhite,kEffectFade));grid.service(pixelTestNow+=150);
  for(uint16_t p=layout.start(2);p<layout.start(2)+layout.count(2);++p)assert(FastLED.pixels[p]==FastLED.pixels[layout.start(2)]);
  grid.setPattern(LedGrid::kPatternRainbow);grid.service(pixelTestNow+=1000);
  for(uint16_t p=layout.used();p<LedGrid::kPixels;++p)assert(FastLED.pixels[p]==CRGB::Black);
  grid.setStaticMode(true);grid.service(pixelTestNow+=1000);
  for(uint8_t group=1;group<=LedGrid::kCells;++group)
    for(uint16_t p=layout.start(group);p<layout.start(group)+layout.count(group);++p)assert(FastLED.pixels[p]==FastLED.pixels[layout.start(group)]);
  uint8_t chunk[3]={25,50,75};assert(grid.stagePixels(0,chunk,1));assert(!grid.configureGroups(layout));
  assert(grid.commitStaged());assert(grid.canConfigureGroups());
#if PAKAR_FIRST_WINS_ENABLED
  for(uint8_t i=0;i<LedGrid::kCells;++i)physical[i]=i+1;
  grid.setPhysicalMapping(physical,LedGrid::kCells);
  grid.configureKeys(7,0xffffff,0x00ff00,0xff0000);grid.showWinner(1);grid.service(pixelTestNow+=1000);
  for(uint16_t p=0;p<LedGrid::kPixels;++p){const uint8_t group=layout.groupAt(p);
    assert(FastLED.pixels[p]==(!group||group>6?CRGB::Black:group<=2?CRGB(0,255,0):CRGB::Red));}
  BoardCore::KeyPresentation style;style.flashes=0;style.fadeMs=0;
  style.groups[3].loseMotion=style.groups[5].loseMotion=BoardCore::KeyPresentation::SharedChase;
  grid.configureKeyPresentation(style);grid.setKeyEffects(true);grid.setStaticMode(false);grid.showWinner(1);
  grid.service(pixelTestNow+=500);
  const uint16_t total=layout.count(4)+layout.count(6);
  for(uint16_t p=0;p<layout.count(4);++p)assert(FastLED.pixels[layout.start(4)+p].r==BoardCore::StateMotion::chase(500,style.chaseMs,p,total));
  for(uint16_t p=0;p<layout.count(6);++p)assert(FastLED.pixels[layout.start(6)+p].r==BoardCore::StateMotion::chase(500,style.chaseMs,layout.count(4)+p,total));
#endif
  puts("Variable-group renderer: physical/logical separation, immutable capture, diagnostics, fades, static/rainbow, tail and key chase passed.");
}
