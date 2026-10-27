#include "PixelCorrectionStore.h"
#include <cassert>
#include <cstdio>
using Store=PixelCorrectionStore<6>;
int main(){
  fs::FS files;Store store;store.begin(files,true);
  JsonDocument draft;assert(!deserializeJson(draft,R"({"enabled":true,"revision":0,"rules":[{"pixel":2,"kind":"remove","offset":0,"enabled":false}]})"));
  assert(!store.save(draft.as<JsonObjectConst>()));assert(store.revision()==1&&store.mapping.source(2)==2);
  const auto writes=files.writes;Store reboot;reboot.begin(files,true);assert(reboot.revision()==1&&reboot.mapping.source(2)==2);
  draft["revision"]=1;draft["rules"][0]["enabled"]=true;draft["ttl_ms"]=250;
  pakarTestMillis=UINT32_MAX-100;assert(!reboot.preview(draft.as<JsonObjectConst>(),millis()));
  assert(reboot.mapping.source(2)==3&&files.writes==writes&&reboot.revision()==1);
  JsonDocument view;assert(!deserializeJson(view,reboot.json()));
  assert(view["sources"][2]==2&&view["active_sources"][2]==3);
  const uint32_t lease=view["preview"]["lease_id"];assert(lease);
  assert(reboot.save(draft.as<JsonObjectConst>())); // Another editor cannot save over live preview.
  assert(reboot.preview(draft.as<JsonObjectConst>(),millis())); // No blind reacquire/retry.
  draft["lease_id"]=lease;pakarTestMillis=20;assert(!reboot.service(millis()));
  pakarTestMillis=149;assert(reboot.service(millis())&&reboot.mapping.source(2)==2&&files.writes==writes);
  assert(reboot.preview(draft.as<JsonObjectConst>(),millis())); // Expired owner cannot renew.
  draft.remove("lease_id");assert(!reboot.preview(draft.as<JsonObjectConst>(),millis()));
  assert(!deserializeJson(view,reboot.json()));const uint32_t newer=view["preview"]["lease_id"];assert(newer!=lease);
  draft["lease_id"]=lease;draft["clear"]=true;assert(reboot.preview(draft.as<JsonObjectConst>(),millis()));
  draft["lease_id"]=newer;assert(!reboot.preview(draft.as<JsonObjectConst>(),millis())&&reboot.mapping.source(2)==2);
  draft.remove("clear");draft.remove("lease_id");assert(!reboot.preview(draft.as<JsonObjectConst>(),millis()));
  assert(!deserializeJson(view,reboot.json()));draft["lease_id"]=view["preview"]["lease_id"].as<uint32_t>();
  files.failRename=true;assert(reboot.save(draft.as<JsonObjectConst>()));assert(reboot.mapping.source(2)==3&&reboot.revision()==1);
  files.failRename=false;assert(!reboot.save(draft.as<JsonObjectConst>())&&reboot.revision()==2);
  assert(!deserializeJson(view,reboot.json()));assert(!view["preview"]["active"].as<bool>());
  Store committed;committed.begin(files,true);assert(committed.revision()==2&&committed.mapping.source(2)==3);
  // Existing enabled-only records remain readable without a new integrity field.
  assert(!deserializeJson(view,*files.files["/pixel-corrections.json"]));
  view.remove("rule_flags_checksum");view["rules"][0].remove("enabled");
  files.files["/pixel-corrections.json"]->clear();serializeJson(view,*files.files["/pixel-corrections.json"]);
  Store legacy;legacy.begin(files,true);assert(legacy.revision()==2&&legacy.mapping.source(2)==3);
  // A disabled flag must be integrity protected; it cannot be smuggled into old checksum-only data.
  view["rules"][0]["enabled"]=false;files.files["/pixel-corrections.json"]->clear();serializeJson(view,*files.files["/pixel-corrections.json"]);
  Store damaged;damaged.begin(files,true);assert(damaged.revision()==0&&damaged.json().find("corrections_damaged")!=String::npos);
  puts("Correction row toggles, durable integrity migration and transient lease ownership/expiry/failed-save restoration passed.");
}
