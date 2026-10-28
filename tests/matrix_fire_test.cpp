#include <MatrixFire.h>
#include "StateAppearanceSettings.h"
#include <cassert>
#include <cstdio>

int main(){
 using Fire=BoardCore::MatrixFire<20,8>;
 Fire fast,slow,reverse,right,left;
 reverse.restart(180);right.restart(90);left.restart(270);
 for(auto*fire:{&fast,&slow,&reverse,&right,&left})fire->advance(UINT32_MAX-63U);
 for(unsigned tick=1;tick<=8;++tick){
  const auto now=uint32_t(UINT32_MAX-63U+tick*16U);
  fast.advance(now);reverse.advance(now);right.advance(now);left.advance(now);
 }
 slow.advance(64); // Eight due steps, including millis rollover.
 unsigned lit=0;
 for(unsigned y=0;y<8;++y)for(unsigned x=0;x<20;++x){
  assert(fast.sample(x,y)==slow.sample(x,y));
  assert(fast.sample(x,y)==reverse.sample(19-x,7-y));
  assert(right.sample(x,y)==left.sample(19-x,7-y));
  lit+=fast.sample(x,y)>0;
 }
 assert(lit>80);assert(fast.sample(20,0)==0&&fast.sample(0,8)==0);
 fast.restart();fast.advance(90000);for(unsigned y=0;y<8;++y)for(unsigned x=0;x<20;++x)assert(!fast.sample(x,y));
 fast.advance(90016);unsigned base=0;for(unsigned x=0;x<20;++x)base+=fast.sample(x,7)>0;
 assert(base>10); // Ignition spans the width, not one strip-end spark.
 fast.advance(UINT32_MAX); // Bounded stall recovery, no unbounded catch-up.
 fs::FS files;StateAppearanceSettings settings;JsonDocument patch;
 const String scene="untouched scene bytes",corrections="untouched corrections bytes";
 files.files["/scene.bin"]=std::make_shared<std::string>(scene);
 files.files["/pixel-corrections.json"]=std::make_shared<std::string>(corrections);
 assert(!deserializeJson(patch,R"({"enabled":true,"fire_rotation":180})"));
 assert(settings.patch(patch.as<JsonVariantConst>())&&settings.value.fireRotation==180);
 assert(settings.save(files));const auto writes=files.writes;assert(settings.save(files)&&files.writes==writes);
 StateAppearanceSettings loaded;assert(loaded.load(files)&&loaded.value.fireRotation==180);
 for(const char*bad:{"-90","45","360","90.5","true","\"90\""}){
  const String json=String("{\"enabled\":false,\"fire_rotation\":")+bad+"}";
  assert(!deserializeJson(patch,json));assert(!settings.patch(patch.as<JsonVariantConst>()));
  assert(settings.value.enabled&&settings.value.fireRotation==180); // All-or-nothing validation.
 }
 // Existing palettes without the additive preference retain zero rotation.
 files.files["/presentation.json"]=std::make_shared<std::string>("{\"enabled\":true,\"fade_ms\":500}");
 StateAppearanceSettings legacy;assert(legacy.load(files)&&legacy.value.fireRotation==0&&legacy.value.fadeMs==500);
 assert(*files.files["/scene.bin"]==scene&&*files.files["/pixel-corrections.json"]==corrections);
 files.failRename=true;patch.clear();patch["fire_rotation"]=90;
 assert(settings.patch(patch.as<JsonVariantConst>())&&!settings.save(files));
 StateAppearanceSettings retained;assert(retained.load(files)&&retained.value.fireRotation==0);
 puts("Fire cold replay, four directions, full-width ignition, rollover/cadence, bounded catch-up and isolated durable rotation passed");
}
