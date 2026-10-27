#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <PixelGroupLayout.h>

// PCController SettingsStore validated-record/no-write-on-load discipline,
// adapted through the existing SceneStore close/readback/atomic-rename owner.
template <class Layout> class PixelGroupStore {
 public:
  Layout layout;
  void begin(fs::FS &filesystem,bool mounted){
    fs_=mounted?&filesystem:nullptr;
    if(!fs_){error_="storage_unavailable";return;}
    if(!fs_->exists(path()))return;
    JsonDocument record;auto file=fs_->open(path(),"r");
    if(!file||file.size()>4096||deserializeJson(record,file)){error_="pixel_groups_damaged";return;}
    Layout candidate;
    if(!record["revision"].is<uint32_t>()||!parse(record.as<JsonObjectConst>(),candidate,record["revision"].as<uint32_t>())||
       !record["checksum"].is<uint32_t>()||record["checksum"].as<uint32_t>()!=checksum(candidate)){
      error_="pixel_groups_damaged";return;
    }
    layout=candidate;
  }
  const char *save(JsonObjectConst request){
    if(!fs_)return "storage_unavailable";
    if(!request["revision"].is<uint32_t>()||request["revision"].as<uint32_t>()!=layout.revision())return "pixel_groups_conflict";
    Layout candidate;
    if(!parse(request,candidate,layout.revision()))return "invalid_pixel_groups";
    if(candidate.sameOverrides(layout)&&!error_)return nullptr;
    if(layout.revision()==UINT32_MAX)return "pixel_groups_revision_exhausted";
    if(!parse(request,candidate,layout.revision()+1))return "invalid_pixel_groups";
    JsonDocument record;encode(record,candidate,false);record["checksum"]=checksum(candidate);
    String bytes;const size_t expected=measureJson(record);
    bytes.reserve(expected);
    if(serializeJson(record,bytes)!=expected||bytes.length()!=expected)return "pixel_groups_encode_failed";
    auto file=fs_->open("/pixel-groups.pending","w");
    if(!file)return "pixel_groups_stage_failed";
    const bool written=file.print(bytes)==bytes.length();file.close();
    file=fs_->open("/pixel-groups.pending","r");
    const bool verified=written&&file&&file.size()==bytes.length()&&file.readString()==bytes;file.close();
    if(!verified)return "pixel_groups_verify_failed";
    if(!fs_->rename("/pixel-groups.pending",path()))return "pixel_groups_commit_failed";
    layout=candidate;error_=nullptr;return nullptr;
  }
  String json(bool compact=false)const{
    JsonDocument record;encode(record,layout,true);if(error_)record["error"]=error_;
    if(compact){record.remove("overrides");record.remove("starts");record["partial"]=true;}
    String bytes;const size_t expected=measureJson(record);bytes.reserve(expected);
    if(serializeJson(record,bytes)!=expected||bytes.length()!=expected)return String();
    return bytes;
  }
  const char *error()const{return error_;}
 private:
  static const char *path(){return "/pixel-groups.json";}
  static bool parse(JsonObjectConst request,Layout &candidate,uint32_t revision){
    if(!request["overrides"].is<JsonArrayConst>())return false;
    if(!request["default_count"].isNull()&&(!request["default_count"].is<uint8_t>()||request["default_count"].as<uint8_t>()!=Layout::defaultCount))return false;
    const auto rows=request["overrides"].as<JsonArrayConst>();
    if(rows.size()>Layout::groupCount)return false;
    uint8_t counts[Layout::groupCount]{};bool seen[Layout::groupCount]{};
    for(JsonVariantConst value:rows){
      if(!value.is<JsonObjectConst>())return false;
      const auto row=value.as<JsonObjectConst>();
      if(!row["group"].is<uint8_t>()||!row["count"].is<uint8_t>())return false;
      const uint8_t group=row["group"].as<uint8_t>();
      if(!group||group>Layout::groupCount||seen[group-1])return false;
      seen[group-1]=true;counts[group-1]=row["count"].as<uint8_t>();
    }
    return candidate.configure(counts,revision);
  }
  static void encode(JsonDocument &record,const Layout &value,bool derived){
    record["revision"]=value.revision();record["default_count"]=Layout::defaultCount;
    auto rows=record["overrides"].to<JsonArray>();
    for(uint16_t group=1;group<=Layout::groupCount;++group)if(value.overrideCount(group)){
      auto row=rows.add<JsonObject>();row["group"]=group;row["count"]=value.overrideCount(group);
    }
    if(!derived)return;
    record["group_count"]=Layout::groupCount;record["capacity"]=Layout::capacity;
    record["used_pixels"]=value.used();record["index_base"]=0;
    auto counts=record["counts"].to<JsonArray>(),starts=record["starts"].to<JsonArray>();
    for(uint16_t group=1;group<=Layout::groupCount;++group){counts.add(value.count(group));starts.add(value.start(group));}
  }
  static uint32_t checksum(const Layout &value){
    uint32_t hash=2166136261UL;
    const auto add=[&](uint8_t byte){hash=(hash^byte)*16777619UL;};
    add(uint8_t(Layout::groupCount));add(Layout::defaultCount);add(uint8_t(Layout::capacity));add(uint8_t(Layout::capacity>>8));
    for(uint8_t shift=0;shift<32;shift+=8)add(value.revision()>>shift);
    for(uint16_t group=1;group<=Layout::groupCount;++group)add(value.overrideCount(group));
    return hash;
  }
  fs::FS *fs_=nullptr;
  const char *error_=nullptr;
};
