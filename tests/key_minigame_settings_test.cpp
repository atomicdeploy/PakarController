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
  puts("Actual minigame settings: defaults, typed atomic patches, persistence/readback, old-file policy, unchanged writes and failed rename passed.");
}
