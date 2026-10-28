#include "HostPresentationStore.h"
#include "StateAppearanceSettings.h"
#include <PixelGroupLayout.h>
#include <CommandReceipts.h>
#include <cassert>
#include <cstdio>
using Store=HostPresentationStore<4,24>;
using Layout=BoardCore::PixelGroupLayout<4,6,24>;
struct Rgb {uint8_t r,g,b;Rgb(uint8_t r=0,uint8_t g=0,uint8_t b=0):r(r),g(g),b(b){}
  bool operator==(const Rgb&o)const{return r==o.r&&g==o.g&&b==o.b;}};
static JsonDocument parse(const char *bytes){JsonDocument doc;assert(!deserializeJson(doc,bytes));return doc;}
int main(){
  fs::FS fs;Store store;Layout layout;
  // Two production stores share LittleFS, not a filename or staging owner.
  const std::string appearance=R"({"enabled":true,"pulse_ms":900,"states":{"won":{"rgb":1193046,"motion":1}}})";
  const std::string appearancePending="unrelated appearance staging bytes";
  fs.files["/presentation.json"]=std::make_shared<std::string>(appearance);
  fs.files["/presentation.pending"]=std::make_shared<std::string>(appearancePending);
  StateAppearanceSettings palette;assert(palette.load(fs));
  store.begin(fs,true,100);
  assert(store.json().find("storage_error")==String::npos);
  const std::string corrections=R"({"revision":31,"enabled":true,"rules":[{"index":4,"shift":-1,"enabled":true}]})";
  const std::string scene="opaque pre-existing scene bytes"; // Unrelated owners are never parsed or rewritten.
  fs.files["/pixel-corrections.json"]=std::make_shared<std::string>(corrections);
  fs.files["/scene.bin"]=std::make_shared<std::string>(scene);
  auto request=parse(R"({"owner":"miniapp","revision":0,"group_layout_revision":0,"lifetime":"temporary","ttl_ms":250,"composite":"overlay","space":"cells","replace":true,"entries":[{"id":1,"rgb":65280}]})");
  pakarTestMillis=UINT32_MAX-100;assert(!store.apply(request.as<JsonObjectConst>(),layout,millis()));
  assert(store.model.lease==101&&fs.writes==0&&store.model.revision==1);
  Rgb pixels[24];for(auto&p:pixels)p=Rgb(255,0,0);const uint8_t mapping[]={4,3,2,1};
  store.model.compose(pixels,layout,mapping,millis());assert(pixels[18]==Rgb(0,255,0)&&pixels[0]==Rgb(255,0,0));
  assert(store.apply(request.as<JsonObjectConst>(),layout,millis())); // Duplicate cannot silently reacquire.
  request["revision"]=1;request["lease_id"]=101;request["owner"]="other";
  assert(store.apply(request.as<JsonObjectConst>(),layout,millis()));assert(store.model.revision==1);
  request["owner"]="miniapp";request["entries"][0]["rgb"]=0x1234567;
  assert(store.apply(request.as<JsonObjectConst>(),layout,millis()));assert(store.model.revision==1);
  pakarTestMillis=149;assert(store.model.service(millis()));assert(!store.model.active()&&fs.writes==0);
  request["revision"]=2;request["lease_id"]=0;request["entries"][0]["rgb"]=0x00ff00;request["lifetime"]="persistent";
  assert(!store.apply(request.as<JsonObjectConst>(),layout,millis()));assert(fs.writes==1&&store.model.savedRevision==3);
  Store reboot;reboot.begin(fs,true,200);assert(reboot.model.saved.enabled&&reboot.model.revision==3);
  request["revision"]=3;assert(!reboot.apply(request.as<JsonObjectConst>(),layout,millis()));
  assert(fs.writes==1&&reboot.model.savedRevision==3); // Explicit identical save does not wear flash.
  request["revision"]=4;request["entries"][0]["rgb"]=0xff0000;fs.failRename=true;
  assert(reboot.apply(request.as<JsonObjectConst>(),layout,millis()));assert(reboot.model.revision==4&&reboot.model.saved.entries[0].rgb==65280);
  fs.failRename=false;request["lifetime"]="temporary";request["composite"]="replace";request["space"]="groups";
  assert(!reboot.apply(request.as<JsonObjectConst>(),layout,millis()));
  for(auto&p:pixels)p=Rgb(255,255,255);
  reboot.model.compose(pixels,layout,mapping,millis());
  assert(pixels[0]==Rgb(255,0,0)&&pixels[18]==Rgb(0,0,0));
  assert(reboot.model.retire()&&!reboot.model.active()&&reboot.model.suppressed); // Direct scene action wins.
  request["revision"]=reboot.model.revision;request["lease_id"]=0;request.remove("entries");
  request["frame_rgb"]="00ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff0000ff00";
  assert(!reboot.apply(request.as<JsonObjectConst>(),layout,millis()));
  reboot.model.compose(pixels,layout,mapping,millis());for(auto&p:pixels)assert(p==Rgb(0,255,0));
  request["revision"]=reboot.model.revision;request["lease_id"]=reboot.model.lease;request["frame_rgb"]="ff";
  assert(reboot.apply(request.as<JsonObjectConst>(),layout,millis()));assert(reboot.model.frameActive);
  request["frame_rgb"]=String(144,'z');assert(reboot.apply(request.as<JsonObjectConst>(),layout,millis()));
  request.remove("frame_rgb");request["replace"]=false;request["entries"].to<JsonArray>();
  const uint32_t frameStarted=reboot.model.temporaryAt;
  pakarTestMillis+=10;assert(!reboot.apply(request.as<JsonObjectConst>(),layout,millis()));
  assert(reboot.model.frameActive&&reboot.model.temporaryAt==frameStarted);
  reboot.model.compose(pixels,layout,mapping,millis());for(auto&p:pixels)assert(p==Rgb(0,255,0));
  request["revision"]=reboot.model.revision;
  request.remove("frame_rgb");request["action"]="clear";assert(!reboot.apply(request.as<JsonObjectConst>(),layout,millis()));
  // Every effect is board-clock driven on variable prefix spans, not fixed6.
  uint8_t overrides[]={5,7,0,0};assert(layout.configure(overrides,9));
  auto effect=parse(R"({"owner":"test","revision":0,"group_layout_revision":9,"lifetime":"temporary","ttl_ms":10000,"replace":true,"space":"groups","composite":"replace","entries":[{"id":2,"rgb":16711680,"period_ms":1000}]})");
  Store animation;animation.begin(fs,false,5);
  for(uint8_t kind=0;kind<=Store::Model::Fade;++kind){
    effect["revision"]=animation.model.revision;effect["lease_id"]=animation.model.lease;
    effect["entries"][0]["effect"]=Store::Model::name(Store::Model::Effect(kind));
    assert(!animation.apply(effect.as<JsonObjectConst>(),layout,0));
    animation.model.compose(pixels,layout,mapping,250);assert(pixels[4]==Rgb()&&pixels[12]==Rgb());
    if(kind!=Store::Model::Solid)assert(animation.model.animated());
  }
  // Fade reaches its exact terminal frame, then ceases perpetual animation.
  assert(animation.model.needsFrame(1001,999));
  animation.model.compose(pixels,layout,mapping,1000);assert(pixels[5]==Rgb(255,0,0));
  assert(!animation.model.needsFrame(1001,1000));
  animation.model.temporary.overlay=true;
  for(auto&p:pixels)p=Rgb(0,0,255);
  animation.model.compose(pixels,layout,mapping,500);assert(pixels[5]==Rgb(127,0,128));
  assert(pixels[4]==Rgb(0,0,255));
  // Immutable original lineage survives cache lookup; later revisions cannot
  // turn a retry into confirmation of another editor's current lease.
  BoardCore::CommandReceipts receipts;BoardCore::CommandReceipts::Entry *slot=nullptr;
  const char *id="0123456789abcdef0123456789abcdef";
  const char *digest="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  assert(receipts.reserve(id,digest,8000,1,slot)==BoardCore::CommandReceipts::Fresh);
  slot->accepted=true;slot->observe(10,123);
  assert(receipts.reserve(id,digest,8000,2,slot)==BoardCore::CommandReceipts::Cached);
  assert(slot->observationMatches(10,123)&&!slot->observationMatches(11,123)&&!slot->observationMatches(10,124));
  // Both full layer descriptions fit the unchanged USB response budget.
  HostPresentationStore<80,480> worst;worst.begin(fs,false,0);
  worst.model.saved.enabled=worst.model.temporary.enabled=true;
  for(auto layer:{&worst.model.saved,&worst.model.temporary})for(auto &s:layer->entries){s.enabled=true;s.rgb=0xffffff;s.periodMs=10000;s.effect=decltype(worst)::Model::Rainbow;s.invert=true;}
  assert(worst.json(true).length()<7168);
  assert(*fs.files.at("/pixel-corrections.json")==corrections&&*fs.files.at("/scene.bin")==scene);
  assert(*fs.files.at("/presentation.json")==appearance);
  assert(*fs.files.at("/presentation.pending")==appearancePending);
  StateAppearanceSettings paletteReboot;assert(paletteReboot.load(fs));
  assert(paletteReboot.value.states[BoardCore::Protocol::kCellWon].rgb==0x123456);
  assert(paletteReboot.value.pulseMs==900);
  const auto hostBytes=*fs.files.at("/host-presentation.json");
  paletteReboot.value.pulseMs=950;assert(paletteReboot.save(fs));
  assert(*fs.files.at("/host-presentation.json")==hostBytes);
  Store hostReboot;hostReboot.begin(fs,true,300);
  assert(hostReboot.model.saved.enabled&&hostReboot.model.saved.entries[0].rgb==65280);
  puts("Host presentation: atomic saved/leased layers, wraparound expiry, owner/revision fencing, full-frame bounds, shared effects, variable groups and USB budget passed.");
}
