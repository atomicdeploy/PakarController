#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <KeyMiniGame.h>

// Same stage/readback/rename transaction as KeyGameSettings. No implicit format,
// no instance migration, and no per-frame persistence or round replay on boot.
struct KeyMiniGameSettings {
  using Game=BoardCore::KeyMiniGame;
  Game::Config value;
  static const char *outputName(Game::OutputSource s){
    constexpr const char *names[]={"disabled","active","success","failure","held","latched","winner","lost"};
    return s<=Game::KeyLost?names[s]:"disabled";
  }
  static const char *toggleName(Game::ToggleKind kind){return kind==Game::Latch?"latch":kind==Game::Momentary?"press":"toggle";}
  bool patch(JsonVariantConst v){
    if(!v.is<JsonObjectConst>())return false;
    auto next=value;
    if(!v["mode"].isUnbound()){
      Game::Mode mode;if(!v["mode"].is<const char*>()||!Game::parse(v["mode"],mode))return false;
      if(mode!=next.mode){const auto mask=next.enabledMask;next=Game::Config::defaults(mode);next.enabledMask=mask;
        // Mode changes retain explicitly configured relay wiring, not timing.
        memcpy(next.outputs,value.outputs,sizeof(next.outputs));}
    }
    auto number=[&](const char *name,uint32_t maximum,auto &out){auto input=v[name];
      if(input.isUnbound())return true;
      if(!input.is<uint32_t>()||input.as<uint32_t>()>maximum)return false;
      out=input.as<uint32_t>();return true;};
    if(!number("limit_ms",86400000,next.limitMs)||!number("warning_ms",86400000,next.warningMs)||
       !number("urgent_ms",86400000,next.urgentMs)||!number("penalty_ms",86400000,next.penaltyMs)||
       !number("window_ms",60000,next.windowMs)||!number("show_ms",10000,next.showMs)||
       !number("gap_ms",5000,next.gapMs)||!number("feedback_ms",5000,next.feedbackMs)||
       !number("rounds",32,next.rounds)||!number("sequence_length",32,next.sequenceLength)||
       !number("palette_size",9,next.paletteSize)||!number("fixed_target",9,next.fixedTarget))return false;
    if(!v["toggle_kind"].isUnbound()){
      if(!v["toggle_kind"].is<const char*>())return false;
      const char *kind=v["toggle_kind"];bool found=false;
      for(uint8_t i=0;i<=Game::Momentary;++i)if(!strcmp(kind,toggleName(Game::ToggleKind(i)))){next.toggleKind=Game::ToggleKind(i);found=true;}
      if(!found)return false;
    }
    if(!v["hard"].isUnbound()){if(!v["hard"].is<bool>())return false;next.hard=v["hard"];}
    if(!v["outputs"].isUnbound()){
      if(!v["outputs"].is<JsonArrayConst>()||v["outputs"].size()!=8)return false;
      for(uint8_t i=0;i<8;++i){auto r=v["outputs"][i];
        if(!r.is<JsonObjectConst>()||!r["source"].is<const char*>()||!r["key"].is<uint8_t>())return false;
        bool found=false;for(uint8_t s=0;s<=Game::KeyLost;++s)if(!strcmp(r["source"],outputName(Game::OutputSource(s)))){
          next.outputs[i]={Game::OutputSource(s),r["key"].as<uint8_t>()};found=true;}
        if(!found)return false;
      }
    }
    if(!next.valid())return false;
    value=next;return true;
  }
  void json(JsonObject out)const{
    const auto &c=value;out["mode"]=Game::name(c.mode);out["limit_ms"]=c.limitMs;
    out["warning_ms"]=c.warningMs;out["urgent_ms"]=c.urgentMs;out["penalty_ms"]=c.penaltyMs;
    out["window_ms"]=c.windowMs;out["show_ms"]=c.showMs;out["gap_ms"]=c.gapMs;
    out["feedback_ms"]=c.feedbackMs;out["rounds"]=c.rounds;out["sequence_length"]=c.sequenceLength;
    out["palette_size"]=c.paletteSize;out["fixed_target"]=c.fixedTarget;
    out["toggle_kind"]=toggleName(c.toggleKind);out["hard"]=c.hard;
    auto a=out["outputs"].to<JsonArray>();for(const auto &r:c.outputs){auto o=a.add<JsonObject>();o["source"]=outputName(r.source);o["key"]=r.key;}
  }
  bool load(fs::FS &fs){auto f=fs.open("/minigame.json","r");if(!f||f.size()>4096)return false;
    JsonDocument d;if(deserializeJson(d,f))return false;return patch(d.as<JsonVariantConst>());}
  bool save(fs::FS &fs)const{
    JsonDocument d;json(d.to<JsonObject>());String data;serializeJson(d,data);
    auto old=fs.open("/minigame.json","r");if(old&&old.readString()==data)return true;old.close();
    auto f=fs.open("/minigame.pending","w");if(!f)return false;
    bool ok=f.print(data)==data.length();f.close();if(!ok)return false;
    f=fs.open("/minigame.pending","r");ok=f&&f.readString()==data;f.close();
    return ok&&fs.rename("/minigame.pending","/minigame.json");
  }
};
