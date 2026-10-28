#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <PixelCorrections.h>

// Reuses SceneStore's staged close/readback/rename discipline and
// PCController SettingsStore's validated-record/unchanged-write avoidance.
// Explicit Save commits once; frames and preview reads never write flash.
template <size_t Pixels> class PixelCorrectionStore {
 public:
  using Mapping=BoardCore::PixelCorrections<Pixels>;
  using Rule=typename Mapping::Rule;
  Mapping mapping;
  void begin(fs::FS &filesystem,bool mounted,uint32_t leaseSeed=0) {
    nextLease_=leaseSeed&0x7fffffffU;
    fs_=mounted?&filesystem:nullptr;
    if(!fs_) { error_="storage_unavailable"; return; }
    if(!fs_->exists(path())) return;
    JsonDocument doc;
    auto file=fs_->open(path(),"r");
    if(!file || file.size()>4096 || deserializeJson(doc,file)) { error_="corrections_damaged"; return; }
    Rule rules[Mapping::maxRules]{};uint8_t count=0;bool enabled=false;
    if(!parse(doc.as<JsonObjectConst>(),rules,count,enabled) || !doc["revision"].is<uint32_t>() ||
       !doc["checksum"].is<uint32_t>() || doc["checksum"].as<uint32_t>()!=checksum(rules,count,enabled,doc["revision"].as<uint32_t>())) {
      error_="corrections_damaged";return;
    }
    const uint32_t revision=doc["revision"].as<uint32_t>();
    if(!doc["rule_flags_checksum"].isNull()) {
      if(!doc["rule_flags_checksum"].is<uint32_t>() ||
         doc["rule_flags_checksum"].as<uint32_t>()!=flagsChecksum(rules,count,enabled,revision)) {
        error_="corrections_damaged";return;
      }
    } else for(uint8_t i=0;i<count;++i)if(!rules[i].enabled){error_="corrections_damaged";return;}
    assign(rules,count,enabled,revision);
  }
  const char *save(JsonObjectConst doc) {
    service(millis());
    if(!fs_) return "storage_unavailable";
    if(!doc["revision"].is<uint32_t>() || doc["revision"].as<uint32_t>()!=revision_)return "corrections_conflict";
    if(previewActive_ && (!doc["lease_id"].is<uint32_t>() || doc["lease_id"].as<uint32_t>()!=leaseId_))return "corrections_preview_owned";
    Rule rules[Mapping::maxRules]{};uint8_t count=0;bool enabled=false;
    if(!parse(doc,rules,count,enabled))return "invalid_corrections";
    bool same=enabled==enabled_&&count==count_;
    for(uint8_t i=0;same&&i<count;++i)same=rules[i].pixel==rules_[i].pixel&&rules[i].kind==rules_[i].kind&&rules[i].offset==rules_[i].offset&&rules[i].enabled==rules_[i].enabled;
    if(same && !error_) {restore();return nullptr;}
    if(revision_==UINT32_MAX)return "corrections_revision_exhausted";
    JsonDocument saved;encode(saved,rules,count,enabled,revision_+1);
    saved["checksum"]=checksum(rules,count,enabled,revision_+1);
    saved["rule_flags_checksum"]=flagsChecksum(rules,count,enabled,revision_+1);
    String bytes;serializeJson(saved,bytes);
    auto file=fs_->open("/pixel-corrections.pending","w");
    if(!file)return "corrections_stage_failed";
    const bool written=file.print(bytes)==bytes.length();file.close();
    file=fs_->open("/pixel-corrections.pending","r");
    const bool verified=written&&file&&file.size()==bytes.length()&&file.readString()==bytes;
    file.close();
    if(!verified)return "corrections_verify_failed";
    if(!fs_->rename("/pixel-corrections.pending",path()))return "corrections_commit_failed";
    assign(rules,count,enabled,revision_+1);error_=nullptr;return nullptr;
  }
  // Leases are RAM-only, loop-owned and fenced against the durable revision.
  // A lost editor cannot leave a correction behind indefinitely, and a delayed
  // release/renewal cannot steal a newer editor's lease.
  const char *preview(JsonObjectConst doc,uint32_t now) {
    service(now);
    if(!doc["revision"].is<uint32_t>() || doc["revision"].as<uint32_t>()!=revision_)return "corrections_conflict";
    if(!doc["lease_id"].isNull()&&!doc["lease_id"].is<uint32_t>())return "invalid_preview_lease";
    const uint32_t requested=doc["lease_id"]|0U;
    if(previewActive_ ? requested!=leaseId_ : requested!=0)return "corrections_preview_owned";
    if(!doc["clear"].isNull()&&!doc["clear"].is<bool>())return "invalid_preview_clear";
    if(doc["clear"]|false){if(!previewActive_)return "corrections_preview_expired";restore();return nullptr;}
    if(!doc["ttl_ms"].isNull()&&!doc["ttl_ms"].is<uint16_t>())return "invalid_preview_ttl";
    const uint16_t ttl=doc["ttl_ms"]|3000U;
    if(ttl<250||ttl>10000)return "invalid_preview_ttl";
    Rule rules[Mapping::maxRules]{};uint8_t count=0;bool enabled=false;
    if(!parse(doc,rules,count,enabled))return "invalid_corrections";
    if(!previewActive_){
      if(nextLease_==UINT32_MAX)return "corrections_lease_exhausted";
      leaseId_=++nextLease_;
    }
    mapping.configure(enabled,rules,count);previewActive_=true;previewAt_=now;previewTtl_=ttl;++activeRevision_;
    return nullptr;
  }
  bool service(uint32_t now) {
    if(!previewActive_ || uint32_t(now-previewAt_)<previewTtl_)return false;
    restore();return true;
  }
  uint32_t activeRevision()const{return activeRevision_;}
  uint32_t previewLease()const{return previewActive_?leaseId_:0;}
  bool previewActive()const{return previewActive_;}
  String previewJson()const {
    JsonDocument doc;encodePreview(doc);String result;serializeJson(doc,result);return result;
  }
  String json() const {
    JsonDocument doc;encode(doc,rules_,count_,enabled_,revision_);
    doc["pixels"]=Pixels;doc["max_rules"]=Mapping::maxRules;doc["index_base"]=0;
    if(error_)doc["error"]=error_;
    Mapping saved;saved.configure(enabled_,rules_,count_);
    auto sources=doc["sources"].template to<JsonArray>();
    for(size_t i=0;i<Pixels;++i)sources.add(saved.source(i));
    auto active=doc["active_sources"].template to<JsonArray>();
    for(size_t i=0;i<Pixels;++i)active.add(mapping.source(i));
    JsonDocument preview;encodePreview(preview);doc["preview"]=preview.as<JsonObjectConst>();
    doc["active_revision"]=activeRevision_;
    String result;serializeJson(doc,result);return result;
  }
  uint32_t revision()const{return revision_;}
 private:
  void encodePreview(JsonDocument &doc)const {
    doc["active"]=previewActive_;doc["lease_id"]=previewActive_?leaseId_:0;
    doc["base_revision"]=revision_;
    const uint32_t age=uint32_t(millis()-previewAt_);
    doc["remaining_ms"]=previewActive_&&age<previewTtl_?previewTtl_-age:0;
  }
  void restore(){
    if(!previewActive_)return;
    previewActive_=false;leaseId_=0;mapping.configure(enabled_,rules_,count_);++activeRevision_;
  }
  static uint32_t flagsChecksum(const Rule *rules,uint8_t count,bool enabled,uint32_t revision){
    uint32_t hash=checksum(rules,count,enabled,revision);
    for(uint8_t i=0;i<count;++i)hash=(hash^uint8_t(rules[i].enabled))*16777619UL;
    return hash;
  }
  static uint32_t checksum(const Rule *rules,uint8_t count,bool enabled,uint32_t revision) {
    uint32_t hash=2166136261UL;
    const auto add=[&](uint8_t byte){hash=(hash^byte)*16777619UL;};
    add(enabled);add(count);
    for(uint8_t b=0;b<4;++b)add(uint8_t(revision>>(8*b)));
    for(uint8_t i=0;i<count;++i){add(uint8_t(rules[i].kind));add(uint8_t(rules[i].pixel));add(uint8_t(rules[i].pixel>>8));
      add(uint8_t(rules[i].offset));add(uint8_t(uint16_t(rules[i].offset)>>8));}
    return hash;
  }
  static const char *path(){return "/pixel-corrections.json";}
  static const char *name(typename Mapping::Kind kind){return kind==Mapping::Black?"black":kind==Mapping::Remove?"remove":"shift";}
  static bool parse(JsonObjectConst doc,Rule *rules,uint8_t &count,bool &enabled) {
    if(!doc["enabled"].is<bool>()||!doc["rules"].is<JsonArrayConst>())return false;
    const auto entries=doc["rules"].as<JsonArrayConst>();
    if(entries.size()>Mapping::maxRules)return false;
    enabled=doc["enabled"].as<bool>();count=0;
    for(JsonObjectConst entry:entries) {
      if(!entry["pixel"].is<uint16_t>()||!entry["kind"].is<const char*>())return false;
      Rule &rule=rules[count++];rule.pixel=entry["pixel"].as<uint16_t>();
      if(!entry["enabled"].isNull()&&!entry["enabled"].is<bool>())return false;
      rule.enabled=entry["enabled"]|true;
      const String kind=entry["kind"].as<String>();
      if(kind=="black")rule.kind=Mapping::Black;
      else if(kind=="remove")rule.kind=Mapping::Remove;
      else if(kind=="shift")rule.kind=Mapping::Shift;
      else return false;
      if(!entry["offset"].isNull()&&!entry["offset"].is<int16_t>())return false;
      rule.offset=entry["offset"]|0;
      if(rule.pixel>=Pixels || rule.offset < -int(Pixels)||rule.offset>int(Pixels)||
         (rule.kind!=Mapping::Shift&&rule.offset))return false;
    }
    return true;
  }
  static void encode(JsonDocument &doc,const Rule *rules,uint8_t count,bool enabled,uint32_t revision) {
    doc["enabled"]=enabled;doc["revision"]=revision;
    auto list=doc["rules"].to<JsonArray>();
    for(uint8_t i=0;i<count;++i){auto row=list.add<JsonObject>();row["kind"]=name(rules[i].kind);row["pixel"]=rules[i].pixel;row["offset"]=rules[i].offset;row["enabled"]=rules[i].enabled;}
  }
  void assign(const Rule *rules,uint8_t count,bool enabled,uint32_t revision) {
    for(uint8_t i=0;i<count;++i)rules_[i]=rules[i];
    count_=count;enabled_=enabled;revision_=revision;previewActive_=false;leaseId_=0;
    mapping.configure(enabled,rules,count);++activeRevision_;
  }
  fs::FS *fs_=nullptr;
  Rule rules_[Mapping::maxRules]{};
  uint8_t count_=0;bool enabled_=true;uint32_t revision_=0;
  const char *error_=nullptr;
  bool previewActive_=false;
  uint32_t leaseId_=0,nextLease_=0,previewAt_=0,activeRevision_=0;
  uint16_t previewTtl_=0;
};
