#include "LedGrid.h"
#include "LedPreferences.h"
#include <cassert>
#include <cstdio>
String withoutPending(const String& frame){
 String value=frame;const auto at=value.find(",\"pending\":");assert(at!=String::npos);
 const auto end=value.find(',',at+1);assert(end!=String::npos);value.erase(at,end-at);return value;
}
void assertPendingSnapshot(const String& previous,const String& pending){
 assert(pending.find("\"pending\":true")!=String::npos);
 assert(withoutPending(previous)==withoutPending(pending));
}
int main(){
 using namespace BoardCore::Protocol;
 LedGrid grid;assert(grid.pixelFrameJson()=="null");grid.begin();
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
 const auto beforeCell=grid.pixelFrameJson();
 grid.setCell(1,kCellRed,kEffectImmediate);assertPendingSnapshot(beforeCell,grid.pixelFrameJson());
 grid.service(++pixelTestNow);assert(grid.pixelFrameJson().find("\"rgb\":\"ff0000ff0000")!=String::npos);
 const uint8_t rgb[]={0x12,0x34,0xab,0xff,0,0};
 const auto before=FastLED.shows;
 const auto beforeStage=grid.pixelFrameJson();assert(grid.stagePixels(474,rgb,2));
 assertPendingSnapshot(beforeStage,grid.pixelFrameJson());assert(!grid.needsRender(++pixelTestNow));
 grid.service(pixelTestNow);assert(FastLED.shows==before); // partial stream never auto-shows
 assert(grid.commitStaged());assert(FastLED.shows==before+1);
 frame=grid.pixelFrameJson();auto at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at+474*6,12)=="1234abff0000");
 assert(!grid.stagePixels(479,rgb,2));assert(grid.pixelFrameJson()!="null");
 uint8_t mapping[80];for(uint8_t i=0;i<80;++i)mapping[i]=i+1;mapping[0]=80;mapping[79]=1;
 const auto beforeMapping=grid.pixelFrameJson();
 grid.setPhysicalMapping(mapping,80);assertPendingSnapshot(beforeMapping,grid.pixelFrameJson());
 grid.service(++pixelTestNow);assert(grid.physicalCell(1)==80);
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at+474*6,6)=="ff0000");
 grid.setPattern(LedGrid::kPatternRainbow);grid.service(++pixelTestNow);
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at,6)!=frame.substr(at+6,6)); // capture actual render buffer, not states
 pixelTestReserve=false;assert(grid.pixelFrameJson()=="null");pixelTestReserve=true;
 assert(grid.stagePixels(0,rgb,2));grid.fill(kCellOff);grid.service(++pixelTestNow);
 assert(grid.pixelFrameJson().find("\"rgb\":\"000000")!=String::npos);
 const auto refresh=FastLED.shows;pixelTestNow+=250;assert(grid.needsRender(pixelTestNow));
 grid.service(pixelTestNow);assert(FastLED.shows==refresh+1);
 const auto beforeOff=grid.pixelFrameJson();grid.setOutputEnabled(false);const auto off=FastLED.shows;
 assertPendingSnapshot(beforeOff,grid.pixelFrameJson());
 grid.setCell(1,kCellRed,kEffectImmediate);grid.setBrightness(77);
 assertPendingSnapshot(beforeOff,grid.pixelFrameJson());pixelTestNow+=300;grid.service(pixelTestNow);
 assert(!grid.needsRender(pixelTestNow)&&FastLED.shows==off&&!grid.outputEnabled());
 assertPendingSnapshot(beforeOff,grid.pixelFrameJson());
 grid.setOutputEnabled(true);grid.service(++pixelTestNow);assert(FastLED.shows==off+1);
 assert(grid.pixelFrameJson().find("\"brightness\":77,\"pending\":false")!=String::npos);
 assert(grid.stagePixels(0,rgb,2));pixelTestNow+=501;grid.service(pixelTestNow);
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at,6)=="000000" && frame.substr(at+474*6,6)=="ff0000"); // Preserve remap; abandon partial RGB.
 assert(grid.stagePixels(0,rgb,1,11));assert(!grid.stagePixels(1,rgb,1,22));
 grid.discardStaged(22);assert(!grid.stagePixels(1,rgb,1,22));
 const auto partialShows=FastLED.shows;
 assert(!grid.commitStaged(0)&&!grid.commitStaged(22)&&!grid.canStage(0));
 assert(FastLED.shows==partialShows);
 grid.discardStaged(11);assert(grid.stagePixels(1,rgb,1,22));assert(grid.commitStaged(22));
 assert(grid.stagePixels(2,rgb,1,11));pixelTestNow+=501;
 assert(!grid.commitStaged(11)); // Expired partial cannot be published by its owner.
 assert(grid.stagePixels(3,rgb,1,22));assert(grid.commitStaged(22));
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at+2*6,6)=="000000"); // Expired owner's partial never leaks into new delta.
 grid.setCell(1,kCellWhite,kEffectImmediate);grid.service(++pixelTestNow);
 grid.setCell(1,kCellRed,kEffectFade);
 const uint8_t green[]={0,255,0};
 assert(grid.stagePixels(474,green,1,33));assert(grid.commitStaged(33));
 pixelTestNow+=300;grid.service(pixelTestNow);
 frame=grid.pixelFrameJson();at=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(at+474*6,6)=="00ff00"); // Old fade cannot repaint a committed raw stream.
 puts("Actual LedGrid: white boot including legacy effects, retained brightness, runtime rainbow, last-published RGB/mapping/brightness/revision through dirty/staging/disabled state with pending flag, partial-frame isolation and allocation failure passed (FastLED/Arduino doubles, not physical output).");
}
