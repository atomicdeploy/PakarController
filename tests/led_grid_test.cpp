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
 assert(grid.staticMode());
 grid.setCell(1,kCellRed,kEffectPulse);grid.service(++pixelTestNow);
 frame=grid.pixelFrameJson();auto staticAt=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(staticAt,36)=="ff0000ff0000ff0000ff0000ff0000ff0000");
 auto staticShows=FastLED.shows;const auto stableStatic=frame;pixelTestNow+=999;
 assert(!grid.needsRender(pixelTestNow));grid.service(pixelTestNow);assert(FastLED.shows==staticShows);
 ++pixelTestNow;assert(grid.needsRender(pixelTestNow));grid.service(pixelTestNow);
 assert(FastLED.shows==staticShows+1);
 frame=grid.pixelFrameJson();const auto staticRgb=frame.find("\"rgb\":\"")+7;
 assert(frame.substr(staticRgb,LedGrid::kPixels*6)==stableStatic.substr(staticAt,LedGrid::kPixels*6));
 assert(grid.pattern()==LedGrid::kPatternNone && grid.state(1)==kCellRed);
 assert(grid.outputJson().find("\"refresh_interval_ms\":1000")!=String::npos);
 grid.setPattern(LedGrid::kPatternFire);grid.service(++pixelTestNow);
 const auto fireFrame=grid.pixelFrameJson();const auto fireAt=fireFrame.find("\"rgb\":\"")+7;
 pixelTestNow+=1000;assert(grid.needsRender(pixelTestNow));grid.service(pixelTestNow);
 frame=grid.pixelFrameJson();
 assert(frame.substr(frame.find("\"rgb\":\"")+7,LedGrid::kPixels*6)==fireFrame.substr(fireAt,LedGrid::kPixels*6));
 assert(grid.pattern()==LedGrid::kPatternFire); // Maintenance is not an animation tick.
 grid.setPattern(LedGrid::kPatternRainbow);grid.service(++pixelTestNow);
 frame=grid.pixelFrameJson();staticAt=frame.find("\"rgb\":\"")+7;
 for(unsigned i=1;i<6;++i)assert(frame.substr(staticAt,6)==frame.substr(staticAt+i*6,6));
 staticShows=FastLED.shows;pixelTestNow+=1000;assert(grid.needsRender(pixelTestNow));
 grid.service(pixelTestNow);assert(FastLED.shows==staticShows+1);
 const uint8_t groupedRgb[]={0x12,0x34,0x56,0xab,0xcd,0xef};
 assert(grid.stagePixels(0,groupedRgb,2));assert(grid.commitStaged());
 frame=grid.pixelFrameJson();staticAt=frame.find("\"rgb\":\"")+7;
 for(unsigned i=0;i<6;++i)assert(frame.substr(staticAt+i*6,6)=="123456");
 grid.setStaticMode(false); // Remaining checks exercise the opt-in effects path.
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
 grid.setOutputEnabled(false);assert(FastLED.shows==off+1); // Reassert physical black even already Off.
 assertPendingSnapshot(beforeOff,grid.pixelFrameJson());
 grid.setCell(1,kCellRed,kEffectImmediate);grid.setBrightness(77);
 assertPendingSnapshot(beforeOff,grid.pixelFrameJson());pixelTestNow+=300;grid.service(pixelTestNow);
 assert(!grid.needsRender(pixelTestNow)&&FastLED.shows==off+1&&!grid.outputEnabled());
 assertPendingSnapshot(beforeOff,grid.pixelFrameJson());
 grid.setOutputEnabled(true);grid.service(++pixelTestNow);assert(FastLED.shows==off+2);
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
 // Entire selection validation is separate from one loop-owned render commit.
 BoardCore::CellBatch selected,invalidSelection;
 assert(BoardCore::CellBatch::parse("1, 2-80,1",9,selected)&&selected.count()==80);
 assert(BoardCore::CellBatch::parse("1,80",4,invalidSelection));
 for(const char *bad:{"","0","81","1,81","4-2","1,","1,,2","1x","1 2","-1","1-80x"}){
   auto previous=invalidSelection;
   assert(!BoardCore::CellBatch::parse(bad,strlen(bad),invalidSelection));
   assert(memcmp(previous.mask,invalidSelection.mask,10)==0); // No partial parse leaks.
 }
 uint8_t wire[12]={1,1};memset(wire+2,255,10);
 BoardCore::CellBatch decoded;assert(BoardCore::CellBatch::decode(wire,12,decoded)&&decoded.count()==80);
 wire[1]=3;assert(!BoardCore::CellBatch::decode(wire,12,decoded));wire[1]=1;
 assert(!BoardCore::CellBatch::decode(wire,11,decoded));
 memset(wire+2,0,10);assert(!BoardCore::CellBatch::decode(wire,12,decoded));
 LedGrid bulk;bulk.begin();bulk.setStaticMode(false);bulk.service(++pixelTestNow);
 const auto whiteFrame=bulk.pixelFrameJson();const auto beforeBatch=FastLED.shows;
 assert(bulk.setCells(selected,kCellRed,kEffectFade));
 assert(FastLED.shows==beforeBatch);assertPendingSnapshot(whiteFrame,bulk.pixelFrameJson());
 for(uint8_t id=1;id<=80;++id)assert(bulk.state(id)==kCellRed);
 pixelTestNow+=150;bulk.service(pixelTestNow);frame=bulk.pixelFrameJson();
 auto batchAt=frame.find("\"rgb\":\"")+7;
 for(unsigned p=0;p<480;++p)assert(frame.substr(batchAt+p*6,6)=="ff8080");
 assert(FastLED.shows==beforeBatch+1); // One full frame, never80 per-cell shows.
 pixelTestNow+=150;bulk.service(pixelTestNow);frame=bulk.pixelFrameJson();batchAt=frame.find("\"rgb\":\"")+7;
 for(unsigned p=0;p<480;++p)assert(frame.substr(batchAt+p*6,6)=="ff0000");
 pixelTestNow=UINT32_MAX-150U;bulk.fill(kCellWhite);bulk.service(pixelTestNow);
 assert(bulk.setCells(selected,kCellRed,kEffectFade));pixelTestNow+=300U;bulk.service(pixelTestNow);
 frame=bulk.pixelFrameJson();batchAt=frame.find("\"rgb\":\"")+7;
 for(unsigned p=0;p<480;++p)assert(frame.substr(batchAt+p*6,6)=="ff0000"); // Clock rollover/late service.
 // A cell action supersedes an unfinished stream, but must not erase unrelated
 // pixels from the previous complete raw frame with stale logical cell targets.
 for(bool grouped:{true,false}){
  LedGrid mixed;mixed.begin();mixed.setStaticMode(grouped);
  const uint8_t retained[]={0x12,0x34,0x56},uncommitted[]={0,255,0};
  assert(mixed.stagePixels(474,retained,1,44)&&mixed.commitStaged(44));
  const auto before=mixed.pixelFrameJson();const auto start=before.find("\"rgb\":\"")+7;
  const auto tail=before.substr(start+474*6,36);
  assert(mixed.stagePixels(475,uncommitted,1,44));
  BoardCore::CellBatch first;first.add(1);
  assert(mixed.setCells(first,kCellRed,kEffectImmediate));mixed.service(++pixelTestNow);
  const auto after=mixed.pixelFrameJson();const auto next=after.find("\"rgb\":\"")+7;
  assert(after.substr(next+474*6,36)==tail);
  for(unsigned p=0;p<6;++p)assert(after.substr(next+p*6,6)=="ff0000");
 }
 const uint8_t lane[]={80,60,40,20,10,30,50,70};
 uint8_t cable[80];bool seen[80]={};
 assert(!BoardCore::TableCellMapping::logicalForPhysical(0)&&!BoardCore::TableCellMapping::physicalForLogical(81));
 for(uint8_t physical=1;physical<=80;++physical){
   const uint8_t logical=lane[(physical-1)%8]-(physical-1)/8;
   assert(logical==BoardCore::TableCellMapping::logicalForPhysical(physical));
   assert(!seen[logical-1]);seen[logical-1]=true;
   assert(BoardCore::TableCellMapping::physicalForLogical(logical)==physical);cable[logical-1]=physical;
 }
 assert(cable[79]==1&&cable[59]==2&&cable[39]==3&&cable[19]==4&&cable[0]==77);
 for(bool grouped:{true,false}){
   LedGrid chase;chase.begin();chase.setPhysicalMapping(cable,80);chase.setStaticMode(grouped);
   pixelTestNow=UINT32_MAX-5100U;chase.setPattern(LedGrid::kPatternCellChase);
   pixelTestNow+=5000U; // Long setup must still publish logical80 first.
   for(uint8_t physical=1;physical<=80;++physical){
     const auto shows=FastLED.shows;chase.service(pixelTestNow);assert(FastLED.shows==shows+1);
     const auto f=chase.pixelFrameJson();const auto start=f.find("\"rgb\":\"")+7;
     const uint8_t expected=81-physical,wireGroup=cable[expected-1];
     for(unsigned p=0;p<480;++p)assert(f.substr(start+p*6,6)==(p/6==unsigned(wireGroup-1)?"ff0000":"ffffff"));
     for(uint8_t id=1;id<=80;++id)assert(chase.state(id)==(id==expected?kCellRed:kCellWhite));
     pixelTestNow+=749U;assert(!chase.needsRender(pixelTestNow));chase.service(pixelTestNow);
     assert(FastLED.shows==shows+1);++pixelTestNow;
   }
   assert(chase.state(1)==kCellRed&&chase.outputJson().find("\"complete\":true")!=String::npos);
   assert(chase.outputJson().find("\"cell_chase\":{\"active\":false")!=String::npos);
   pixelTestNow+=60000U;chase.service(pixelTestNow);assert(chase.state(1)==kCellRed); // No loop/restart.
   chase.setPattern(LedGrid::kPatternCellChase);chase.service(pixelTestNow);
   assert(chase.state(80)==kCellRed);
   chase.setOutputEnabled(false);pixelTestNow+=10000U;chase.service(pixelTestNow);
   assert(chase.state(80)==kCellRed);chase.setOutputEnabled(true);chase.service(pixelTestNow);
   assert(chase.state(80)==kCellRed);pixelTestNow+=749U;chase.service(pixelTestNow);assert(chase.state(80)==kCellRed);
   pixelTestNow+=5000U;chase.service(pixelTestNow);
   assert(chase.state(79)==kCellRed); // Late loop advances only one, never skips.
   chase.setCell(1,kCellRed,kEffectImmediate);assert(chase.pattern()==LedGrid::kPatternNone);
   chase.setPattern(LedGrid::kPatternRainbow);assert(chase.pattern()==LedGrid::kPatternRainbow);
 }
 // Atomic diagnostics: full clear, exact raw tail, no stale group expansion.
 LedGrid diagnostic;diagnostic.begin();
 for(bool off:{false,true})for(uint8_t count:{uint8_t(1),uint8_t(6)}) {
   const uint16_t index=count==1?479:474;
   const auto shows=FastLED.shows;
   assert(diagnostic.diagnostic(index,count,0x1234ab,off));
   assert(FastLED.shows==shows+1 && !diagnostic.staticMode() && diagnostic.outputEnabled());
   const auto f=diagnostic.pixelFrameJson();const auto start=f.find("\"rgb\":\"")+7;
   for(unsigned p=0;p<480;++p)
     assert(f.substr(start+p*6,6)==(!off && p>=index && p<index+count?"1234ab":"000000"));
   pixelTestNow+=1000U;diagnostic.service(pixelTestNow);
   assert(diagnostic.pixelFrameJson().find("\"rgb\":\""+f.substr(start,2880))!=String::npos);
 }
 const auto unchanged=diagnostic.pixelFrameJson();
 assert(!diagnostic.diagnostic(480,1,0xffffff,false));
 assert(!diagnostic.diagnostic(479,6,0xffffff,false));
 assert(!diagnostic.diagnostic(1,6,0xffffff,false));
 assert(!diagnostic.diagnostic(0,2,0xffffff,false));
 assert(!diagnostic.diagnostic(0,1,0x1000000,false));
 assert(diagnostic.pixelFrameJson()==unchanged);
 uint8_t bytes[3]={1,2,3};assert(diagnostic.stagePixels(0,bytes,1,77));
 assert(!diagnostic.diagnostic(0,1,0xff0000,false));
 diagnostic.discardStaged(77);
 diagnostic.setOutputEnabled(false);
 assert(diagnostic.diagnostic(0,1,0xff0000,false)&&diagnostic.outputEnabled());
  puts("Actual LedGrid: exact80-cell cable bijection,6-pixel atomic chase/rollover/no-skip/stop at1, bulk transitions, white boot, snapshots and partial-frame isolation passed (host doubles, not optical output).");
}
