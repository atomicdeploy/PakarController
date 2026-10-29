#include <cassert>
#include <cstdio>
#include "KeyMiniGameSettings.h"
static bool patch(KeyMiniGameSettings &settings,const char *text){
  JsonDocument document;assert(!deserializeJson(document,text));
  return settings.patch(document.as<JsonVariantConst>());
}
int main(){
  using G=BoardCore::KeyMiniGame;
  fs::FS files;KeyMiniGameSettings settings;settings.value.enabledMask=63;
  assert(!settings.load(files)&&settings.value.valid());
  assert(!settings.bootLast&&settings.segmentAnimation&&settings.timerBeeps&&settings.timerAlternateTones);
  assert(patch(settings,R"({"mode":"simon","boot_mode":"last","segment_animation":false,"timer_beeps":false,"timer_alternate_tones":false})"));
  settings.value.outputs[0]={G::KeyHeld,2};
  assert(settings.save(files));const auto writes=files.writes;
  assert(settings.save(files)&&files.writes==writes); // Identical settings do not wear flash.
  KeyMiniGameSettings restored;restored.value.enabledMask=63;
  assert(restored.load(files)&&restored.bootLast&&!restored.segmentAnimation&&!restored.timerBeeps&&!restored.timerAlternateTones);
  assert(restored.value.mode==G::Simon&&restored.value.enabledMask==63&&restored.value.outputs[0].key==2);
  for(const char *bad:{R"({"boot_mode":"last-winner"})",R"({"timer_beeps":1})",
      R"({"boot_mode":"first","timer_beeps":true,"rounds":0})",R"({"segment_animation":null})"}){
    assert(!patch(restored,bad));assert(restored.bootLast&&!restored.timerBeeps&&!restored.segmentAnimation);
  }
  // A boot choice changes neither the saved last game nor any board wiring.
  const auto first=BoardCore::bootGameConfig(restored.value,false);
  assert(first.mode==G::PressToWin&&!first.limitMs&&first.enabledMask==63&&first.outputs[0].key==2);
  assert(restored.value.mode==G::Simon&&files.writes==writes);
  assert(patch(restored,R"({"boot_mode":"first","segment_animation":true,"timer_beeps":true,"timer_alternate_tones":true})"));
  files.failRename=true;assert(!restored.save(files));files.failRename=false;
  KeyMiniGameSettings previous;assert(previous.load(files)&&previous.bootLast&&!previous.timerBeeps);
  assert(restored.save(files));KeyMiniGameSettings saved;assert(saved.load(files)&&!saved.bootLast&&saved.timerBeeps);
  // Older files gain defaults in RAM, never an automatic rewrite/migration.
  *files.files["/minigame.json"]=R"({"mode":"toggle"})";
  const auto oldWrites=files.writes;KeyMiniGameSettings older;
  assert(older.load(files)&&!older.bootLast&&older.segmentAnimation&&older.timerBeeps&&older.timerAlternateTones);
  assert(files.writes==oldWrites);
  // Color Pattern now opens immediately; Matching Pairs still previews by
  // default. Explicit saved choices remain authoritative across load/save.
  KeyMiniGameSettings colors;colors.value.enabledMask=63;
  assert(patch(colors,R"({"mode":"color-match"})"));
  assert(!colors.value.introEnabled&&colors.value.introMs==500);
  assert(colors.save(files));KeyMiniGameSettings cold;
  assert(cold.load(files)&&!cold.value.introEnabled&&cold.value.mode==G::ColorMatch);
  assert(patch(cold,R"({"intro_enabled":true,"intro_ms":1000})")&&cold.save(files));
  KeyMiniGameSettings custom;assert(custom.load(files)&&custom.value.introEnabled&&custom.value.introMs==1000);
  assert(patch(custom,R"({"mode":"memory-pairs"})")&&custom.value.introEnabled);
  assert(custom.save(files));KeyMiniGameSettings pairs;
  assert(pairs.load(files)&&pairs.value.introEnabled&&pairs.value.mode==G::MemoryPairs);
  assert(patch(pairs,R"({"mode":"illuminated"})")&&pairs.value.windowMs==10000);
  assert(patch(pairs,R"({"window_ms":20000})")&&pairs.save(files));
  KeyMiniGameSettings window;assert(window.load(files)&&window.value.windowMs==20000);
  assert(!patch(window,R"({"window_ms":99})")&&window.value.windowMs==20000);
  KeyMiniGameSettings simon;simon.value.enabledMask=63;
  assert(patch(simon,R"({"mode":"simon"})"));
  assert(simon.value.rounds==3&&simon.value.sequenceLength==3&&simon.value.sequenceGrowth==2);
  assert(simon.value.showMs==300&&simon.value.gapMs==120&&simon.value.feedbackMs==240);
  assert(simon.save(files));KeyMiniGameSettings newSimon;
  assert(newSimon.load(files)&&newSimon.value.sequenceGrowth==2&&newSimon.value.rounds==3);
  assert(patch(newSimon,R"({"sequence_growth":0})")&&newSimon.save(files));
  KeyMiniGameSettings fixed;assert(fixed.load(files)&&fixed.value.sequenceGrowth==0);
  for(const char *bad:{R"({"sequence_growth":32})",R"({"sequence_growth":-1})",R"({"sequence_growth":1.5})",R"({"sequence_growth":null})"}){
    assert(!patch(fixed,bad)&&fixed.value.sequenceGrowth==0);
  }
  // Old explicit round counts, step counts, pacing and +1 growth are retained
  // in RAM. Loading does not silently migrate or rewrite the saved instance.
  *files.files["/minigame.json"]=R"({"mode":"simon","rounds":2,"sequence_length":8,"show_ms":700,"gap_ms":300,"feedback_ms":320})";
  const auto legacyWrites=files.writes;KeyMiniGameSettings legacy;
  assert(legacy.load(files)&&legacy.value.rounds==2&&legacy.value.sequenceLength==8&&legacy.value.sequenceGrowth==1);
  assert(legacy.value.showMs==700&&legacy.value.gapMs==300&&legacy.value.feedbackMs==320&&files.writes==legacyWrites);
  assert(legacy.save(files));KeyMiniGameSettings kept;
  assert(kept.load(files)&&kept.value.sequenceGrowth==1&&kept.value.sequenceLength==8);
  puts("Actual minigame settings: defaults, typed atomic patches, persistence/readback, old-file policy, unchanged writes and failed rename passed.");
}
