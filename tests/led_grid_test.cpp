#include "LedGrid.h"
#include "LedPreferences.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace BoardCore::Protocol;
 LedGrid grid;grid.begin();
 auto frame=grid.pixelFrameJson();
 assert(frame.find("\"revision\":1")!=String::npos);
 assert(frame.find("\"rgb\":\"ffffff")!=String::npos);
 assert(frame.find("\"physical\":[1,2,3")!=String::npos);
 // Boot policy accepts existing files without restoring a saved animation.
 for (uint8_t pattern=LedGrid::kPatternNone;pattern<=LedGrid::kPatternFire;++pattern) {
   auto saved=whiteBootPreferences(192);saved.pattern=pattern;
   saved.check=ledPreferencesCheck(saved);
   grid.setPattern(LedGrid::kPatternRainbow);
   assert(applyLedBootPreferences(grid,saved));grid.service(++pixelTestNow);
   assert(grid.pattern()==LedGrid::kPatternNone && grid.brightness()==192);
   frame=grid.pixelFrameJson();const auto start=frame.find("\"rgb\":\"")+7;
   assert(frame.substr(start,LedGrid::kPixels*6)==String(LedGrid::kPixels*6,'f'));
   for(uint8_t cell=1;cell<=80;++cell)assert(grid.state(cell)==kCellWhite);
 }
 auto invalid=whiteBootPreferences(80);invalid.check^=1;
 assert(!applyLedBootPreferences(grid,invalid));assert(grid.brightness()==192);
 invalid=whiteBootPreferences(0);assert(!applyLedBootPreferences(grid,invalid));
 invalid=whiteBootPreferences(80);invalid.pattern=3;invalid.check=ledPreferencesCheck(invalid);
 assert(!applyLedBootPreferences(grid,invalid));
 const auto saved=whiteBootPreferences(grid.brightness());
 assert(saved.pattern==LedGrid::kPatternNone && saved.check==ledPreferencesCheck(saved));
 grid.setCell(1,kCellRed,kEffectImmediate);assert(grid.pixelFrameJson()=="null");
 grid.service(++pixelTestNow);assert(grid.pixelFrameJson().find("\"rgb\":\"ff0000ff0000")!=String::npos);
 const uint8_t rgb[]={0x12,0x34,0xab,0xff,0,0};
 const auto before=FastLED.shows;
 assert(grid.stagePixels(474,rgb,2));
 assert(grid.pixelFrameJson()=="null");assert(!grid.needsRender(++pixelTestNow));
 grid.service(pixelTestNow);assert(FastLED.shows==before); // partial stream never auto-shows
 grid.showStaged();assert(FastLED.shows==before+1);
 frame=grid.pixelFrameJson();auto at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at+474*6,12)=="1234abff0000");
 assert(!grid.stagePixels(479,rgb,2));assert(grid.pixelFrameJson()!="null");
 uint8_t mapping[80];for(uint8_t i=0;i<80;++i)mapping[i]=i+1;mapping[0]=80;mapping[79]=1;
 grid.setPhysicalMapping(mapping,80);assert(grid.pixelFrameJson()=="null");
 grid.service(++pixelTestNow);assert(grid.physicalCell(1)==80);
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at+474*6,6)=="ff0000");
 grid.setPattern(LedGrid::kPatternRainbow);grid.service(++pixelTestNow);
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at,6)!=frame.substr(at+6,6)); // capture actual render buffer, not states
 pixelTestReserve=false;assert(grid.pixelFrameJson()=="null");pixelTestReserve=true;
 assert(grid.stagePixels(0,rgb,2));grid.fill(kCellOff);grid.service(++pixelTestNow);
 assert(grid.pixelFrameJson().find("\"rgb\":\"000000")!=String::npos);
 puts("Actual LedGrid: white boot including legacy effects, retained brightness, explicit runtime rainbow, committed RGB/mapping, partial-frame isolation and allocation failure passed (FastLED/Arduino doubles, not physical output).");
}
