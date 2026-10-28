#include "LedGrid.h"
#include "LedPreferences.h"
#include "CloudAnimationProgress.h"
#include <StateMotion.h>
#include <cassert>
#include <cstdio>
unsigned panelNotifications=0;uint8_t panelKind=0;uint16_t panelValue=0;
void panelNotice(uint8_t kind,uint16_t value,uint8_t){++panelNotifications;panelKind=kind;panelValue=value;}
void hostPresentationComposition(){
  using namespace BoardCore::Protocol;
  const auto savedNow=pixelTestNow;LedGrid grid;LedGrid::Presentation host;grid.begin();grid.setPresentation(&host);
  const auto rgb=[&](){const auto f=grid.pixelFrameJson();return f.substr(f.find("\"rgb\":\"")+7,LedGrid::kPixels*6);};
  const auto base=rgb();host.temporary.enabled=true;host.temporary.overlay=true;host.temporary.physical=true;
  host.temporary.entries[0].enabled=true;host.temporary.entries[0].rgb=0x00ff00;host.revision=1;
  grid.presentationChanged();grid.service(++pixelTestNow);
  assert(rgb().substr(0,36)==String("00ff0000ff0000ff0000ff0000ff0000ff00"));
  assert(rgb().substr(36)==base.substr(36));for(uint8_t id=1;id<=80;++id)assert(grid.state(id)==kCellWhite);
  host.temporary.entries[0].effect=LedGrid::Presentation::Ring;grid.presentationChanged();
  pixelTestNow+=50;assert(grid.needsRender(pixelTestNow));grid.service(pixelTestNow);
  const auto first=rgb();pixelTestNow+=50;grid.service(pixelTestNow);assert(rgb()!=first); // Even GPIO-static shares explicit host effects.
  assert(grid.resetWhite());assert(!host.active()&&host.suppressed);grid.service(++pixelTestNow);assert(rgb()==base);
  assert(grid.pixelFrameJson().find("\"presentation_revision\":2")!=String::npos);
  pixelTestNow=savedNow;
}
String withoutPending(const String& frame){
 String value=frame;const auto at=value.find(",\"pending\":");assert(at!=String::npos);
 const auto end=value.find(',',at+1);assert(end!=String::npos);value.erase(at,end-at);return value;
}
void assertPendingSnapshot(const String& previous,const String& pending){
 assert(pending.find("\"pending\":true")!=String::npos);
 assert(withoutPending(previous)==withoutPending(pending));
}
void rainbowClockCadence(){
 using namespace BoardCore::Protocol;
 const uint32_t savedNow=pixelTestNow;
 const auto rgb=[](const LedGrid &grid){const auto f=grid.pixelFrameJson();return f.substr(f.find("\"rgb\":\"")+7,LedGrid::kPixels*6);};
 const auto hue=[&rgb](const LedGrid &grid){return unsigned(std::stoul(rgb(grid).substr(0,2),nullptr,16));};
 LedGrid fast,delayed;pixelTestNow=1000;fast.begin();delayed.begin();
 fast.setStaticMode(false);delayed.setStaticMode(false);
 fast.setPattern(LedGrid::kPatternRainbow);delayed.setPattern(LedGrid::kPatternRainbow);
 fast.service(pixelTestNow);delayed.service(pixelTestNow);
 assert(hue(fast)==0 && rgb(fast)==rgb(delayed)); // First frame has no synthetic +3 jump.
 const auto initial=rgb(fast);const auto repeatedShows=FastLED.shows;
 assert(fast.needsRender(pixelTestNow));fast.service(pixelTestNow);
 assert(rgb(fast)==initial && FastLED.shows==repeatedShows+1); // Speed is not an FPS cap.
 for(unsigned i=1;i<=10;++i){pixelTestNow=1000+i*16;fast.service(pixelTestNow);}
 delayed.service(pixelTestNow);assert(rgb(fast)==rgb(delayed) && hue(fast)==30);
 delayed.setPattern(LedGrid::kPatternRainbow);pixelTestNow+=16;delayed.service(pixelTestNow);
 assert(hue(delayed)==33); // Re-selecting the active effect does not reset its clock.
 delayed.setPattern(LedGrid::kPatternNone);pixelTestNow+=1000;
 delayed.setPattern(LedGrid::kPatternRainbow);delayed.service(pixelTestNow);
 assert(hue(delayed)==33); // A new effect continues the last hue, without a step on start.
 pixelTestNow+=16;delayed.service(pixelTestNow);assert(hue(delayed)==36);
 LedGrid rollover;pixelTestNow=UINT32_MAX-31U;rollover.begin();
 rollover.setPattern(LedGrid::kPatternRainbow);rollover.service(pixelTestNow);
 pixelTestNow+=64U;rollover.service(pixelTestNow);assert(hue(rollover)==12);
 const auto grouped=rgb(rollover);for(unsigned i=1;i<6;++i)assert(grouped.substr(0,6)==grouped.substr(i*6,6));
 // Large elapsed values must widen before multiplication and wrap only the hue.
 LedGrid longRunning;pixelTestNow=17;longRunning.begin();longRunning.setPattern(LedGrid::kPatternRainbow);
 pixelTestNow+=UINT32_MAX-1U;longRunning.service(pixelTestNow);
 assert(hue(longRunning)==uint8_t((uint64_t(UINT32_MAX-1U)*3U)/16U));
 LedGrid stationary;stationary.begin();stationary.setCell(1,kCellRed,kEffectImmediate);stationary.service(pixelTestNow);
 const auto still=rgb(stationary);pixelTestNow+=2000;stationary.service(pixelTestNow);
 assert(stationary.pattern()==LedGrid::kPatternNone && rgb(stationary)==still);
 pixelTestNow=savedNow;
}
void matrixFireRendering(){
 const auto savedNow=pixelTestNow;
 const auto rgb=[](const LedGrid &g){const auto f=g.pixelFrameJson();return f.substr(f.find("\"rgb\":\"")+7,LedGrid::kPixels*6);};
 const String black(LedGrid::kPixels*6,'0');
 uint8_t mapping[80];for(uint8_t i=0;i<80;++i)mapping[i]=BoardCore::TableCellMapping::physicalForLogical(i+1);
 for(bool grouped:{true,false})for(uint16_t rotation:{0,90,180,270}){
  LedGrid grid;grid.begin();grid.setPhysicalMapping(mapping,80);grid.setStaticMode(grouped);
  BoardCore::StateAppearance appearance;appearance.fireRotation=rotation;
  assert(grid.configureAppearance(appearance));assert(grid.setPattern(LedGrid::kPatternFire));
  pixelTestNow+=1000;grid.service(pixelTestNow);assert(rgb(grid)==black);
  const uint32_t start=pixelTestNow;BoardCore::MatrixFire<20,8> expected;expected.restart(rotation);expected.advance(start);
  for(unsigned tick=1;tick<=32;++tick){
   pixelTestNow=start+tick*16;assert(grid.needsRender(pixelTestNow));grid.service(pixelTestNow);expected.advance(pixelTestNow);
   const auto frame=rgb(grid);
   for(uint8_t cell=0;cell<80;++cell){
    const uint8_t heat=expected.sample(2*(cell%10)+(cell/10)%2,cell/10);
    const uint16_t physical=mapping[cell]-1;
    const char hex[]="0123456789abcdef";
    String color;color+=hex[heat>>4];color+=hex[heat&15];color+="0000"; // Host HeatColor double.
    for(unsigned pixel=0;pixel<6;++pixel)assert(frame.substr((physical*6+pixel)*6,6)==color);
    assert(grid.state(cell+1)==BoardCore::Protocol::kCellWhite);
   }
  }
  assert(rgb(grid)!=black);
  const auto frame=rgb(grid);grid.service(pixelTestNow);assert(rgb(grid)==frame); // No synthetic step per render.
  assert(grid.setPattern(LedGrid::kPatternFire));grid.service(pixelTestNow);assert(rgb(grid)==black); // Same-effect replay.
  grid.setPattern(LedGrid::kPatternNone);grid.setPattern(LedGrid::kPatternFire);grid.service(pixelTestNow);assert(rgb(grid)==black);
 }
 LedGrid grid;grid.begin();auto layout=grid.groupLayout();uint8_t counts[80]{};counts[0]=3;counts[1]=9;
 assert(layout.configure(counts,9));assert(grid.configureGroups(layout));
 grid.setPattern(LedGrid::kPatternFire);grid.service(++pixelTestNow);pixelTestNow+=16;grid.service(pixelTestNow);
 assert(grid.groupLayout().count(1)==3&&grid.groupLayout().count(2)==9); // Never normalize user groups.
 pixelTestNow=savedNow;
}
void cloudAnimationProgress(){
 const auto savedNow=pixelTestNow;
 LedGrid grid;pixelTestNow=10;grid.begin();grid.setPattern(LedGrid::kPatternRainbow);
 const auto initial=grid.pixelFrameJson();const auto before=FastLED.shows;
 // Simulate a two-second cloud burst which calls progress between its writes:
 // the real renderer advances throughout it, not once after returning to loop.
 for(unsigned i=1;i<=20;++i){pixelTestNow=10+i*100;assert(PakarLed::serviceDuringCloud(grid,pixelTestNow,false,false,true));}
 assert(FastLED.shows==before+20&&grid.pixelFrameJson()!=initial);
 const auto shown=FastLED.shows;
 assert(!PakarLed::serviceDuringCloud(grid,++pixelTestNow,true,false,true));
 assert(!PakarLed::serviceDuringCloud(grid,++pixelTestNow,false,true,true));
 assert(!PakarLed::serviceDuringCloud(grid,++pixelTestNow,false,false,false));
 assert(FastLED.shows==shown);
 grid.setOutputEnabled(false);const auto disabled=FastLED.shows;
 assert(!PakarLed::serviceDuringCloud(grid,++pixelTestNow,false,false,true));
 assert(FastLED.shows==disabled);pixelTestNow=savedNow;
}
void sharedStateMotion(){
 using BoardCore::StateMotion;
 // Same integer waveform as the existing keycontroller, for both group sizes.
 for(uint32_t period : {100U,701U,900U,1800U}){
   for(uint32_t t=0;t<period*2;++t){
     const auto phase=t%period,half=period/2;
     const auto distance=phase<half?phase:period-phase;
     assert(StateMotion::pulse(t,period)==uint8_t(255-105*distance/half));
     for(uint16_t sectors : {6,12}){
       for(uint16_t i=0;i<sectors;++i){
         const auto around=(phase+period-uint32_t(i)*period/sectors)%period;
         const auto d=around<period-around?around:period-around;
         for(bool neighbors : {false,true})for(bool invert : {false,true}){
           const auto reach=(period/sectors)*(neighbors?2U:1U);
           const auto halo=d<reach?uint8_t(255*d/reach):255;
           const auto expected=invert?uint8_t(255-75*halo/255):uint8_t(130+125*halo/255);
           assert(StateMotion::ring(t,period,i,sectors,neighbors,invert)==expected);
         }
         const auto head=phase*sectors/period;
         const auto trail=(head+sectors-i)%sectors;
         assert(StateMotion::chase(t,period,i,sectors)==(trail<4?uint8_t(255-trail*45):60));
       }
     }
   }
 }
 assert(StateMotion::pulse(100,0)==255 && StateMotion::pulse(100,1)==255);
 assert(StateMotion::ring(100,0,0,6,true,false)==255);
 assert(StateMotion::ring(100,900,0,0,true,false)==255);
 assert(StateMotion::chase(100,0,0,6)==255);
 assert(StateMotion::chase(100,900,6,6)==255);
}
void semanticAppearance(){
 using namespace BoardCore::Protocol;
 LedGrid grid;grid.begin();
 BoardCore::StateAppearance palette;
 palette.states[kCellLost].rgb=0x123456;
 palette.states[kCellIdle].rgb=0xabcdef;
 assert(grid.configureAppearance(palette));
 assert(grid.setCell(1,kCellLost,kEffectImmediate));
 assert(grid.setCell(2,kCellWon,kEffectImmediate));
 grid.service(++pixelTestNow);
 const auto rgb=[](const LedGrid &g){const auto f=g.pixelFrameJson();return f.substr(f.find("\"rgb\":\"")+7,LedGrid::kPixels*6);};
 assert(rgb(grid).substr(0,6)=="123456");
 assert(rgb(grid).substr(36,6)=="00ff00");
 grid.setRedLock(true);
 assert(!grid.setCell(1,kCellIdle,kEffectImmediate)); // State, not red RGB.
 BoardCore::CellBatch selected;selected.add(1);
 const auto original=rgb(grid);
 assert(grid.preview(selected,0x102030,100));
 grid.service(++pixelTestNow);
 assert(rgb(grid).substr(0,6)=="102030" && grid.state(1)==kCellLost);
 assert(grid.redLock() && !grid.setCell(1,kCellIdle,kEffectImmediate));
 pixelTestNow+=100;assert(grid.needsRender(pixelTestNow));grid.service(pixelTestNow);
 assert(rgb(grid)==original && !grid.previewActive(pixelTestNow));
 assert(!grid.preview(selected,0x1000000,100));
 assert(!grid.preview(selected,0,0));assert(!grid.preview(selected,0,10001));
 const auto saved=grid.appearance().states[kCellLost].rgb;
 palette.states[kCellOff].rgb=1;
 assert(!grid.configureAppearance(palette));assert(grid.appearance().states[kCellLost].rgb==saved);
}
void repeatedFadeKeepsDeadline(){
 using namespace BoardCore::Protocol;
 const auto savedNow=pixelTestNow;
 const auto rgb=[](const LedGrid &g){const auto f=g.pixelFrameJson();return f.substr(f.find("\"rgb\":\"")+7,LedGrid::kPixels*6);};
 for(const uint32_t start:{1000U,UINT32_MAX-180U}){
  LedGrid grid;pixelTestNow=start;grid.begin();grid.setStaticMode(false);
  assert(grid.setCell(1,kCellRed,kEffectFade));
  pixelTestNow=start+100U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="ffaaaa");
  assert(grid.setCell(1,kCellRed,kEffectFade));
  pixelTestNow=start+200U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="ff5555");
  assert(grid.setCell(1,kCellRed,kEffectFade));
  pixelTestNow=start+300U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="ff0000");
  // A different target starts from the last published color, not white or black.
  assert(grid.setCell(1,kCellWhite,kEffectFade));
  pixelTestNow+=150U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="ff8080");
  assert(grid.setCell(1,kCellWhite,kEffectImmediate));
  grid.service(++pixelTestNow);assert(rgb(grid).substr(0,6)=="ffffff");
  // Pulse is intentional replay and must retain that behavior.
  assert(grid.setCell(1,kCellWhite,kEffectPulse));
  pixelTestNow+=100U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="555555");
  // A different effect is a new intent even when its state/target is identical.
  assert(grid.setCell(1,kCellWhite,kEffectFade));
  pixelTestNow+=150U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="aaaaaa");
  assert(grid.setCell(1,kCellWhite,kEffectFade));
  pixelTestNow+=150U;grid.service(pixelTestNow);assert(rgb(grid).substr(0,6)=="ffffff");
 }
 pixelTestNow=savedNow;
}
int main(){
 matrixFireRendering();
  hostPresentationComposition();
 repeatedFadeKeepsDeadline();
 semanticAppearance();
 sharedStateMotion();
 using namespace BoardCore::Protocol;
 rainbowClockCadence();
 cloudAnimationProgress();
 LedGrid grid;assert(grid.pixelFrameJson()=="null");grid.begin();
 assert(grid.outputCapabilitiesJson()=="{\"backend\":\"fastled\",\"rmt\":false,\"dma\":false}");
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
 assert(frame.substr(frame.find("\"rgb\":\"")+7,LedGrid::kPixels*6)!=fireFrame.substr(fireAt,LedGrid::kPixels*6));
 assert(grid.pattern()==LedGrid::kPatternFire); // Explicit Fire animates in static grouped mode too.
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
 unsigned diagnosticRevision=0;
 for(bool off:{false,true})for(uint8_t count:{uint8_t(1),uint8_t(6)}) {
   const uint16_t index=count==1?479:474;
   const auto shows=FastLED.shows;
   assert(diagnostic.diagnostic(index,count,0x1234ab,off));
   assert(FastLED.shows==shows+1 && diagnostic.staticMode() && diagnostic.outputEnabled());
   ++diagnosticRevision;
   assert(diagnostic.outputJson().find("\"diagnostic\":{\"revision\":"+std::to_string(diagnosticRevision)+",\"group_layout_revision\":0,\"active\":true")!=String::npos);
   assert(diagnostic.outputJson().find("\"index\":"+std::to_string(index)+",\"count\":"+std::to_string(count)+",\"rgb\":1193131,\"off\":"+(off?"true":"false"))!=String::npos);
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
 diagnostic.showWinner(1);
 assert(diagnostic.outputJson().find("\"active\":false")!=String::npos);
 {
   // Advanced diagnostics are one transient post-render layer, not a scene.
   LedGrid layer;layer.begin();layer.setCell(1,kCellRed,kEffectImmediate);layer.service(++pixelTestNow);
   const auto compact=layer.pixelFrameJson(true);
   assert(compact.size()<LedGrid::kCells*10U+200U&&compact.find("\"rgb\"")==String::npos&&compact.find("\"physical\"")==String::npos);
   assert(compact.find("\"group_layout_revision\":0")!=String::npos&&compact.find("\"group_counts\":[6,6,")!=String::npos&&compact.find("\"group_starts\":[0,6,")!=String::npos);
   assert(compact.find("\"revision\":"+std::to_string(layer.frameRevision()))!=String::npos);
   assert(compact.find("\"pending\":false")!=String::npos);
   const auto raw=[](const String &frame){return frame.substr(frame.find("\"rgb\":\"")+7,480*6);};
   const auto base=raw(layer.pixelFrameJson());
   BoardCore::PixelDiagnostic options;options.composite=BoardCore::PixelDiagnostic::Overlay;
   assert(layer.diagnostic(479,1,0x00ff00,false,options));
   auto rgb=raw(layer.pixelFrameJson());assert(rgb.substr(0,479*6)==base.substr(0,479*6));
   assert(rgb.substr(479*6)=="00ff00"&&layer.state(1)==kCellRed&&layer.staticMode());
   options.effect=BoardCore::PixelDiagnostic::Blink;options.periodMs=1000;options.dutyPercent=25;
   const auto began=pixelTestNow;assert(layer.diagnostic(474,6,0xff0000,false,options));
   pixelTestNow=began+250;assert(layer.needsRender(pixelTestNow));layer.service(pixelTestNow);
   rgb=raw(layer.pixelFrameJson());assert(rgb.substr(474*6)==String(36,'0'));
   assert(rgb.substr(0,474*6)==base.substr(0,474*6));
   options.effect=BoardCore::PixelDiagnostic::Breathe;options.periodMs=1001;
   assert(options.gain(0)==0&&options.gain(500)==255&&options.gain(1001)==0);
   assert(layer.diagnostic(474,6,0xff0000,false,options));
   pixelTestNow+=500;layer.service(pixelTestNow);assert(raw(layer.pixelFrameJson()).substr(474*6)==String("ff0000ff0000ff0000ff0000ff0000ff0000"));
   options.effect=BoardCore::PixelDiagnostic::Rainbow;
   assert(layer.diagnostic(474,6,0xff0000,false,options));const auto first=raw(layer.pixelFrameJson());
   pixelTestNow+=300;layer.service(pixelTestNow);rgb=raw(layer.pixelFrameJson());assert(rgb!=first);
   for(unsigned i=475;i<480;++i)assert(rgb.substr(i*6,6)==rgb.substr(474*6,6));
   options.enabled=false;assert(layer.diagnostic(474,6,0xff0000,false,options));
   assert(!layer.diagnosticActive()&&raw(layer.pixelFrameJson())==base);
   layer.setPattern(LedGrid::kPatternRainbow);options.enabled=true;
   assert(layer.diagnostic(0,1,0xff0000,false,options));pixelTestNow+=100;layer.service(pixelTestNow);
   assert(layer.pattern()==LedGrid::kPatternRainbow&&layer.state(1)==kCellRed);
   options.enabled=false;assert(layer.diagnostic(0,1,0xff0000,false,options));assert(layer.pattern()==LedGrid::kPatternRainbow);
   layer.setOutputEnabled(false);options.enabled=true;
   assert(layer.diagnostic(0,1,0xff0000,false,options)&&layer.outputEnabled()&&!layer.sceneOutputEnabled());
   options.enabled=false;assert(layer.diagnostic(0,1,0xff0000,false,options)&&!layer.outputEnabled());
   assert(raw(layer.pixelFrameJson())==String(480*6,'0')&&layer.pixelFrameJson().find("\"pending\":false")!=String::npos);
   options.enabled=true;options.periodMs=99;const auto untouched=layer.pixelFrameJson();
   assert(!layer.diagnostic(0,1,0xff0000,false,options)&&layer.pixelFrameJson()==untouched);
 }
 assert(diagnostic.diagnostic(474,6,0x1234ab,false));
 diagnostic.setPattern(LedGrid::kPatternRainbow);
 assert(diagnostic.outputJson().find("\"active\":false")!=String::npos);
 {
   LedGrid maintenance;maintenance.begin();maintenance.setStaticMode(false);
   maintenance.setPattern(LedGrid::kPatternRainbow);pixelTestNow+=1000;maintenance.service(pixelTestNow);
   auto rgbOf=[](const String &f){return f.substr(f.find("\"rgb\":\"")+7,480*6);};
   const auto original=rgbOf(maintenance.pixelFrameJson());
   PakarUpdate::Lifecycle life;const uint32_t started=pixelTestNow;
   assert(life.observe(PakarUpdate::Source::Local,1,"prepare",0,0,true,false,started));
   pixelTestNow+=25;maintenance.presentMaintenance(life,pixelTestNow);
   const auto faded=rgbOf(maintenance.pixelFrameJson());
   assert(faded.substr(0,6)!=faded.substr(6,6)); // Preserve each pixel, not six flattened copies.
   pixelTestNow=started+700;maintenance.presentMaintenance(life,pixelTestNow);
   assert(maintenance.maintenancePresented());
   assert(rgbOf(maintenance.pixelFrameJson())==String(480*6,'0'));
   const auto shows=FastLED.shows;pixelTestNow+=100;maintenance.presentMaintenance(life,pixelTestNow);
   assert(FastLED.shows==shows); // One terminal black, not a stream of black writes.
   assert(maintenance.pattern()==LedGrid::kPatternRainbow && maintenance.state(1)==kCellWhite);
   life.observe(PakarUpdate::Source::Local,1,"receiving",240,480,true,false,pixelTestNow);
   pixelTestNow+=25;maintenance.presentMaintenance(life,pixelTestNow);
   assert(!maintenance.maintenancePresented());
   life.observe(PakarUpdate::Source::Local,1,"complete",480,480,false,true,pixelTestNow);
   pixelTestNow+=1700;maintenance.presentMaintenance(life,pixelTestNow);
   assert(maintenance.pattern()==LedGrid::kPatternRainbow && maintenance.outputEnabled());
   maintenance.setPattern(LedGrid::kPatternNone);maintenance.fill(kCellWhite);
   maintenance.service(++pixelTestNow);assert(rgbOf(maintenance.pixelFrameJson())==String(480*6,'f'));
   maintenance.setOutputEnabled(false);
   life.observe(PakarUpdate::Source::Local,2,"prepare",0,0,true,false,pixelTestNow);
   const auto disabledShows=FastLED.shows;pixelTestNow+=800;maintenance.presentMaintenance(life,pixelTestNow);
   assert(!maintenance.outputEnabled() && FastLED.shows==disabledShows && maintenance.maintenancePresented());
   assert(original.size()==480*6);
 }
  grid.setPanelObserver(panelNotice);
  BoardCore::CellBatch all;for(uint8_t id=1;id<=80;++id)all.add(id);
  assert(grid.setCells(all,kCellOff,kEffectImmediate));
  assert(panelNotifications==1&&panelKind==12&&panelValue==kCellOff);
  grid.service(++pixelTestNow);
  const auto black=grid.pixelFrameJson();const auto start=black.find("\"rgb\":\"")+7;
  assert(black.substr(start,480*6)==String(480*6,'0'));
  assert(grid.setCell(2,kCellRed,kEffectImmediate));assert(panelKind==1&&panelValue==2);
  grid.setRedLock(true);assert(grid.redLock());
  const auto beforeLocked=grid.pixelFrameJson();const auto beforeNotice=panelNotifications;
  assert(!grid.setCells(all,kCellWhite,kEffectImmediate));
  assert(grid.state(1)==kCellOff&&grid.state(2)==kCellRed);
  assert(!grid.resetWhite()&&beforeLocked==grid.pixelFrameJson()&&beforeNotice==panelNotifications);
  const uint8_t ids[]={1,2},newStates[]={kCellRed,kCellWhite};
  assert(!grid.setAssignments(ids,newStates,2));assert(grid.state(1)==kCellOff);
  const uint8_t duplicate[]={1,1};assert(!grid.setAssignments(duplicate,newStates,2));
  const uint8_t invalidStates[]={kCellRed,4};assert(!grid.setAssignments(ids,invalidStates,2));
  assert(grid.setCell(2,kCellOff,kEffectImmediate)); // Lock forbids red->white, not Off.
  grid.setRedLock(false);const auto assignmentNotices=panelNotifications;assert(grid.setAssignments(ids,newStates,2));
  assert(panelNotifications==assignmentNotices+1); // One transaction, not one AVR notice per row.
  assert(grid.state(1)==kCellRed&&grid.state(2)==kCellWhite);
  assert(grid.setPattern(LedGrid::kPatternCellChase));
  uint8_t chaseStates[80];for(uint8_t id=1;id<=80;++id)chaseStates[id-1]=grid.state(id);
  grid.setRedLock(true);assert(grid.pattern()==LedGrid::kPatternNone);
  pixelTestNow+=1000;grid.service(pixelTestNow);
  for(uint8_t id=1;id<=80;++id)assert(grid.state(id)==chaseStates[id-1]);
  assert(!grid.setPattern(LedGrid::kPatternCellChase));
  assert(grid.setPattern(LedGrid::kPatternRainbow));assert(grid.redLock());
  puts("Actual LedGrid: cable mapping,6-pixel cells, full480 maintenance black, per-pixel fade, scene preservation and disabled-output isolation passed (host doubles, not optical output).");
}
