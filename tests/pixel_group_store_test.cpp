#include "PixelGroupStore.h"
#include <cassert>
#include <cstdio>
using Layout=BoardCore::PixelGroupLayout<80,6,480>;
using Store=PixelGroupStore<Layout>;
int main(){
  fs::FS files;Store store;store.begin(files,true);
  assert(!files.writes&&store.layout.used()==480&&store.layout.revision()==0);
  JsonDocument request;assert(!deserializeJson(request,R"({"revision":0,"overrides":[{"group":2,"count":7},{"group":1,"count":4}]})"));
  assert(!store.save(request.as<JsonObjectConst>()));assert(store.layout.revision()==1&&store.layout.used()==479);
  assert(store.layout.start(2)==4&&store.layout.count(2)==7);
  auto writes=files.writes;Store reboot;reboot.begin(files,true);
  assert(!reboot.error()&&reboot.layout.sameOverrides(store.layout)&&reboot.layout.revision()==1&&files.writes==writes);
  assert(reboot.save(request.as<JsonObjectConst>())); // Stale revision never writes.
  request["revision"]=1;assert(!reboot.save(request.as<JsonObjectConst>())&&files.writes==writes);
  for(const char *invalid:{R"({"revision":1,"overrides":[{"group":1,"count":5},{"group":1,"count":4}]})",
      R"({"revision":1,"overrides":[{"group":0,"count":5}]})",R"({"revision":1,"overrides":[{"group":81,"count":5}]})",
      R"({"revision":1,"overrides":[{"group":1,"count":256}]})",R"({"revision":1,"overrides":[{"group":1,"count":-1}]})",
      R"({"revision":1,"overrides":[{"group":1,"count":1.5}]})",R"({"revision":1,"overrides":[{"group":1,"count":true}]})",
      R"({"revision":1,"overrides":[{"group":1,"count":7}]})",R"({"revision":1,"overrides":[false]})",
      R"({"revision":1,"default_count":7,"overrides":[]})",R"({"revision":1})"}){
    JsonDocument bad;assert(!deserializeJson(bad,invalid));assert(reboot.save(bad.as<JsonObjectConst>()));
    assert(files.writes==writes&&reboot.layout.revision()==1);
  }
  request["overrides"][0]["count"]=6;
  files.failWrite=true;assert(reboot.save(request.as<JsonObjectConst>()));files.failWrite=false;
  assert(reboot.layout.revision()==1);Store writeFailure;writeFailure.begin(files,true);assert(writeFailure.layout.revision()==1);
  files.failRename=true;assert(reboot.save(request.as<JsonObjectConst>()));files.failRename=false;
  assert(reboot.layout.revision()==1);Store renameFailure;renameFailure.begin(files,true);assert(renameFailure.layout.revision()==1);
  assert(!reboot.save(request.as<JsonObjectConst>())&&reboot.layout.revision()==2);
  // Full replacement and explicit zero inherit. Canonical persisted rows sort
  // by physical group and exclude inherited zero, not by logical UI ordering.
  assert(!deserializeJson(request,R"({"revision":2,"default_count":6,"overrides":[{"group":1,"count":0}]})"));
  assert(!reboot.save(request.as<JsonObjectConst>())&&reboot.layout.revision()==3&&!reboot.layout.customized());
  JsonDocument full,compact;assert(!deserializeJson(full,reboot.json()));assert(!deserializeJson(compact,reboot.json(true)));
  assert(full["counts"].size()==80&&full["starts"].size()==80&&full["overrides"].size()==0);
  assert(compact["partial"].as<bool>()&&compact["counts"].size()==80&&compact["starts"].isNull()&&compact["overrides"].isNull());
  assert(full["used_pixels"]==480&&full["capacity"]==480&&full["index_base"]==0);
  writes=files.writes;
  *files.files["/pixel-groups.json"]="{broken";
  Store corrupt;corrupt.begin(files,true);assert(corrupt.error()&&corrupt.layout.revision()==0&&files.writes==writes);
  Store unmounted;unmounted.begin(files,false);assert(unmounted.error()&&unmounted.save(request.as<JsonObjectConst>()));
  // A record is role-bound by checksum: table counts never become key groups.
  fs::FS roles;Store table;table.begin(roles,true);request["revision"]=0;
  assert(!deserializeJson(request,R"({"revision":0,"overrides":[{"group":1,"count":4}]})"));
  assert(!table.save(request.as<JsonObjectConst>()));
  PixelGroupStore<BoardCore::PixelGroupLayout<18,12,216>> keys;keys.begin(roles,true);
  assert(keys.error()&&keys.layout.used()==216);
  puts("Group layout persistence: atomic/readback failures, conflicts, bounds, corruption, no-op writes and compact metadata passed.");
}
