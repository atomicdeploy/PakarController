#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <HostPresentation.h>

// PCController SettingsStore's explicit validated record + unchanged-write
// discipline, using the existing staged readback/rename filesystem owner.
template<uint16_t Groups,uint16_t Pixels> class HostPresentationStore {
 public:
  using Model=BoardCore::HostPresentation<Groups,Pixels>;
  using Layer=typename Model::Layer;
  Model model;
  void begin(fs::FS &fs,bool mounted,uint32_t seed){
    fs_=mounted?&fs:nullptr;nextLease_=seed&0x7fffffffU;
    if(!fs_){error_="storage_unavailable";return;}
    if(!fs_->exists(path()))return;
    auto file=fs_->open(path(),"r");JsonDocument record;Layer candidate;
    if(!file||file.size()>16384||deserializeJson(record,file)||
      !record["revision"].is<uint32_t>()||!readLayer(record.as<JsonObjectConst>(),candidate,true)||
      !record["checksum"].is<uint32_t>()||record["checksum"].as<uint32_t>()!=checksum(candidate,record["revision"].as<uint32_t>())){
      error_="presentation_damaged";return;
    }
    model.saved=candidate;model.revision=model.savedRevision=record["revision"].as<uint32_t>();
  }
  template<class Layout> const char *apply(JsonObjectConst request,const Layout &layout,uint32_t now){
    model.service(now);
    if(!request["revision"].is<uint32_t>()||request["revision"].as<uint32_t>()!=model.revision)return "presentation_conflict";
    if(!request["group_layout_revision"].is<uint32_t>()||request["group_layout_revision"].as<uint32_t>()!=layout.revision())return "pixel_group_layout_changed";
    if(!validOwner(request["owner"]))return "invalid_presentation_owner";
    const char *owner=request["owner"];
    const char *lifetime=request["lifetime"]|"";
    const bool persistent=!strcmp(lifetime,"persistent");
    if(!persistent&&strcmp(lifetime,"temporary"))return "invalid_presentation_lifetime";
    if(!request["lease_id"].isNull()&&!request["lease_id"].is<uint32_t>())return "invalid_presentation_lease";
    const uint32_t lease=request["lease_id"]|0U;
    if(model.temporary.enabled?(lease!=model.lease||strcmp(owner,model.temporary.owner)):lease!=0)return "presentation_owned";
    if(model.revision==UINT32_MAX)return "presentation_revision_exhausted";
    const char *action=request["action"]|"set";
    if(strcmp(action,"set")&&strcmp(action,"clear"))return "invalid_presentation_action";
    if(!strcmp(action,"clear")){
      if(persistent){Layer empty;const char *error=save(empty,model.revision+1);if(error)return error;
        model.saved=empty;model.suppressed=false;}
      else if(!model.temporary.enabled)return "presentation_lease_expired";
      model.release();return nullptr;
    }
    Layer candidate=persistent?model.saved:model.temporary;
    const bool replace=request["replace"]|false;
    if(!request["replace"].isNull()&&!request["replace"].is<bool>())return "invalid_presentation_replace";
    if(replace)candidate.clear();
    if(!readLayer(request,candidate,false))return "invalid_presentation";
    // A logical map is a permutation of these physical groups; requiring the
    // longest span keeps every Ring valid even if names/mapping later change.
    uint8_t longest=0;for(uint16_t group=1;group<=Groups;++group)if(layout.count(group)>longest)longest=layout.count(group);
    for(const auto &style:candidate.entries)if(style.enabled&&style.effect==Model::Ring&&style.periodMs<longest)return "presentation_period_too_short";
    candidate.enabled=true;memset(candidate.owner,0,sizeof(candidate.owner));strcpy(candidate.owner,owner);
    const auto frame=request["frame_rgb"];
    if(!frame.isNull()){
      if(persistent||!frame.is<const char*>()||!request["entries"].isUnbound()||!replace||candidate.overlay)return "invalid_presentation_frame";
      const auto text=frame.as<JsonString>();
      if(text.size()!=size_t(layout.used())*6U)return "invalid_presentation_frame";
      for(size_t i=0;i<text.size();++i)if(hex(text.c_str()[i])<0)return "invalid_presentation_frame";
    }
    if(persistent){
      const char *error=save(candidate,model.revision+1);if(error)return error;
      model.saved=candidate;model.savedAt=now;
      model.suppressed=false;model.release();return nullptr;
    }
    if(!request["ttl_ms"].is<uint16_t>())return "invalid_presentation_ttl";
    const uint16_t ttl=request["ttl_ms"];if(ttl<250||ttl>10000)return "invalid_presentation_ttl";
    if(!model.temporary.enabled&&nextLease_==UINT32_MAX)return "presentation_lease_exhausted";
    if(!model.temporary.enabled||replace)model.temporaryAt=now;
    if(!model.temporary.enabled)model.lease=++nextLease_;
    const bool retainFrame=model.frameActive&&!replace&&frame.isNull()&&
      request["entries"].is<JsonArrayConst>()&&request["entries"].as<JsonArrayConst>().size()==0&&
      candidate.overlay==model.temporary.overlay&&candidate.physical==model.temporary.physical;
    model.temporary=candidate;model.startedAt=now;model.ttlMs=ttl;model.frameActive=retainFrame||!frame.isNull();
    if(!frame.isNull()){const char *text=frame;for(uint16_t i=0;i<layout.used()*3U;++i)model.frame[i]=(hex(text[i*2])<<4)|hex(text[i*2+1]);}
    ++model.revision;return nullptr;
  }
  String json(bool full=false)const{
    JsonDocument doc;doc["revision"]=model.revision;doc["saved_revision"]=model.savedRevision;
    doc["active"]=model.active();doc["suspended"]=model.suppressed;doc["persistent"]=model.saved.enabled;
    doc["temporary"]=model.temporary.enabled;doc["lease_id"]=model.lease;
    doc["owner"]=model.temporary.enabled?model.temporary.owner:model.saved.owner;
    const uint32_t age=uint32_t(millis()-model.startedAt);
    doc["remaining_ms"]=model.temporary.enabled&&age<model.ttlMs?model.ttlMs-age:0;
    doc["frame"]=model.frameActive;if(error_)doc["storage_error"]=error_;
    if(full){
      encodeCompact(doc["saved"].to<JsonObject>(),model.saved);
      encodeCompact(doc["live"].to<JsonObject>(),model.temporary);
      auto limits=doc["limits"].to<JsonObject>();limits["entries"]=Groups;limits["pixels"]=Pixels;
      limits["ttl_min_ms"]=250;limits["ttl_max_ms"]=10000;limits["period_min_ms"]=100;limits["period_max_ms"]=10000;
      limits["request_bytes"]=4096;limits["style_encoding"]="id,rgb,effect,period_ms,flags";
      auto effects=limits["effects"].to<JsonArray>();for(uint8_t i=0;i<=Model::Fade;++i)effects.add(Model::name(typename Model::Effect(i)));
    }
    String result;const size_t size=measureJson(doc);result.reserve(size);
    return serializeJson(doc,result)==size?result:String();
  }
 private:
  static bool validOwner(JsonVariantConst value){
    if(!value.is<const char*>())return false;
    const char *s=value;
    const size_t size=strlen(s);if(!size||size>32)return false;
    for(size_t i=0;i<size;++i)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||s[i]=='-'||s[i]=='_'||s[i]=='.'))return false;
    return true;
  }
  static bool readLayer(JsonObjectConst request,Layer &candidate,bool record){
    const char *space=request["space"]|(candidate.physical?"groups":"cells");
    if(strcmp(space,"groups")&&strcmp(space,"cells"))return false;
    const bool physical=!strcmp(space,"groups");
    if(candidate.enabled&&candidate.physical!=physical)return false;
    candidate.physical=physical;
    const char *composite=request["composite"]|(candidate.overlay?"overlay":"replace");
    if(strcmp(composite,"overlay")&&strcmp(composite,"replace"))return false;
    candidate.overlay=!strcmp(composite,"overlay");
    if(record){if(!request["enabled"].is<bool>()||(!validOwner(request["owner"])&&request["enabled"].as<bool>()))return false;
      candidate.enabled=request["enabled"];const char *owner=request["owner"]|"";if(strlen(owner)>32)return false;
      memset(candidate.owner,0,sizeof(candidate.owner));strcpy(candidate.owner,owner);}
    if(!request["entries"].is<JsonArrayConst>())return !record&&request["frame_rgb"].is<const char*>();
    const auto rows=request["entries"].as<JsonArrayConst>();if(rows.size()>Groups)return false;
    bool seen[Groups]{};
    for(JsonVariantConst value:rows){
      if(!value.is<JsonObjectConst>())return false;
      auto row=value.as<JsonObjectConst>();
      if(!row["id"].is<uint16_t>())return false;
      const uint16_t id=row["id"];if(!id||id>Groups||seen[id-1])return false;seen[id-1]=true;
      auto &style=candidate.entries[id-1];
      if(!row["enabled"].isNull()&&!row["enabled"].is<bool>())return false;
      style.enabled=row["enabled"]|true;if(!style.enabled){style.clear();continue;}
      if(!row["rgb"].is<uint32_t>())return false;
      style.rgb=row["rgb"];
      if(!row["effect"].isNull()&&!row["effect"].is<const char*>())return false;
      if(!Model::parse(row["effect"]|"solid",style.effect))return false;
      if(!row["period_ms"].isNull()&&!row["period_ms"].is<uint16_t>())return false;
      style.periodMs=row["period_ms"]|1000U;
      for(const char *key:{"neighbors","invert"})if(!row[key].isNull()&&!row[key].is<bool>())return false;
      style.neighbors=row["neighbors"]|true;style.invert=row["invert"]|false;if(!style.valid())return false;
    }
    return true;
  }
  static void encode(JsonObject out,const Layer &layer){
    out["enabled"]=layer.enabled;out["owner"]=layer.owner;out["space"]=layer.physical?"groups":"cells";out["composite"]=layer.overlay?"overlay":"replace";
    auto rows=out["entries"].to<JsonArray>();for(uint16_t i=0;i<Groups;++i)if(layer.entries[i].enabled){
      const auto &s=layer.entries[i];auto row=rows.add<JsonObject>();row["id"]=i+1;row["rgb"]=s.rgb;
      row["effect"]=Model::name(s.effect);row["period_ms"]=s.periodMs;row["neighbors"]=s.neighbors;row["invert"]=s.invert;}
  }
  static void encodeCompact(JsonObject out,const Layer &layer){
    out["enabled"]=layer.enabled;out["owner"]=layer.owner;out["space"]=layer.physical?"groups":"cells";out["composite"]=layer.overlay?"overlay":"replace";
    auto rows=out["styles"].to<JsonArray>();
    for(uint16_t i=0;i<Groups;++i)if(layer.entries[i].enabled){
      const auto &s=layer.entries[i];auto row=rows.add<JsonArray>();
      row.add(i+1);row.add(s.rgb);row.add(uint8_t(s.effect));row.add(s.periodMs);row.add(uint8_t((s.neighbors?1:0)|(s.invert?2:0)));
    }
  }
  static uint32_t checksum(const Layer &layer,uint32_t revision){
    uint32_t h=2166136261UL;const auto add=[&](uint8_t b){h=(h^b)*16777619UL;};
    for(uint8_t s=0;s<32;s+=8)add(revision>>s);
    add(layer.enabled);add(layer.overlay);add(layer.physical);
    for(const char c:layer.owner)add(c);
    for(const auto &s:layer.entries){add(s.enabled);for(uint8_t n=0;n<24;n+=8)add(s.rgb>>n);add(s.periodMs);add(s.periodMs>>8);add(s.effect);add(s.neighbors);add(s.invert);}return h;
  }
  const char *save(const Layer &layer,uint32_t revision){
    if(!fs_)return "storage_unavailable";
    // Unchanged explicit saves resume the layer without re-writing flash.
    if(checksum(layer,model.savedRevision)==checksum(model.saved,model.savedRevision)&&!error_)return nullptr;
    JsonDocument record;encode(record.to<JsonObject>(),layer);record["revision"]=revision;record["checksum"]=checksum(layer,revision);
    String bytes;const size_t expected=measureJson(record);bytes.reserve(expected);
    if(serializeJson(record,bytes)!=expected)return "presentation_encode_failed";
    auto file=fs_->open("/presentation.pending","w");if(!file)return "presentation_stage_failed";
    const bool written=file.print(bytes)==bytes.length();file.close();file=fs_->open("/presentation.pending","r");
    const bool verified=written&&file&&file.size()==bytes.length()&&file.readString()==bytes;file.close();
    if(!verified)return "presentation_verify_failed";
    if(!fs_->rename("/presentation.pending",path()))return "presentation_commit_failed";
    model.savedRevision=revision;error_=nullptr;return nullptr;
  }
  static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
  static const char *path(){return "/presentation.json";}
  fs::FS *fs_=nullptr;uint32_t nextLease_=0;const char *error_=nullptr;
};
