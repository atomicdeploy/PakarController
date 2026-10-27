#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <AudioCues.h>
#include <KeyPresentation.h>
#ifndef PAKAR_INITIAL_KEY_MASK
#define PAKAR_INITIAL_KEY_MASK 511
#endif
static_assert(PAKAR_INITIAL_KEY_MASK >= 1 && PAKAR_INITIAL_KEY_MASK <= 511,
              "initial key mask must select one to nine boxes");
// ESP owns persistent game configuration. AVR receives the enabled mask over
// the existing acknowledged link; it must not guess configuration after reset.
// Retains SettingsStore's unchanged-write avoidance and SceneStore's
// stage/readback/rename commit (never truncate the last committed settings).
struct KeyGameSettings {
  BoardCore::KeyPresentation presentation;
  bool patchPresentation(JsonVariantConst input){
    if(!input.is<JsonObjectConst>())return false;
    auto next=presentation;
    auto number=[&](const char *name,uint16_t &value,uint16_t low,uint16_t high){
      auto v=input[name];if(v.isNull())return true;
      if(!v.is<uint16_t>()||v.as<uint16_t>()<low||v.as<uint16_t>()>high)return false;
      value=v.as<uint16_t>();return true;
    };
    if(!number("flash_on_ms",next.flashOnMs,50,5000)||!number("flash_off_ms",next.flashOffMs,50,5000)||
       !number("last_flash_on_ms",next.lastFlashOnMs,0,5000)||
       !number("fade_ms",next.fadeMs,0,10000)||!number("pulse_ms",next.pulseMs,100,10000)||
       !number("chase_ms",next.chaseMs,100,20000))return false;
    if(!input["flashes"].isNull()){
      if(!input["flashes"].is<uint8_t>()||input["flashes"].as<uint8_t>()>10)return false;
      next.flashes=input["flashes"];
    }
    if(!input["groups"].isNull()){
      if(!input["groups"].is<JsonArrayConst>())return false;
      uint32_t seen=0;
      for(JsonObjectConst g:input["groups"].as<JsonArrayConst>()){
        if(!g["id"].is<uint8_t>())return false;
        uint8_t id=g["id"];if(id<1||id>18||(seen&(1UL<<(id-1))))return false;
        seen|=1UL<<(id-1);auto &out=next.groups[id-1];
        if(!g["override_colors"].isNull()){
          if(!g["override_colors"].is<bool>())return false;
          out.overrideColors=g["override_colors"];
        }
        for(const char *name:{"win_rgb","lose_rgb"})if(!g[name].isNull()){
          if(!g[name].is<uint32_t>()||g[name].as<uint32_t>()>0xffffff)return false;
          if(strcmp(name,"win_rgb")==0)out.win=g[name];else out.lose=g[name];
        }
        for(const char *name:{"win_motion","lose_motion"})if(!g[name].isNull()){
          if(!g[name].is<uint8_t>()||g[name].as<uint8_t>()>3)return false;
          if(strcmp(name,"win_motion")==0)out.winMotion=g[name];else out.loseMotion=g[name];
        }
      }
    }
    presentation=next;return true;
  }
  void presentationJson(JsonObject out)const{
    const auto &p=presentation;
    out["flash_on_ms"]=p.flashOnMs;out["flash_off_ms"]=p.flashOffMs;out["flashes"]=p.flashes;
    out["last_flash_on_ms"]=p.lastFlashOnMs;
    out["fade_ms"]=p.fadeMs;out["pulse_ms"]=p.pulseMs;out["chase_ms"]=p.chaseMs;
    auto groups=out["groups"].to<JsonArray>();
    for(uint8_t i=0;i<18;++i){auto g=groups.add<JsonObject>();const auto &v=p.groups[i];
      g["id"]=i+1;g["override_colors"]=v.overrideColors;g["win_rgb"]=v.win;g["lose_rgb"]=v.lose;
      g["win_motion"]=v.winMotion;g["lose_motion"]=v.loseMotion;}
  }
  uint16_t enabledMask=PAKAR_INITIAL_KEY_MASK;
  bool extendedEffects=false;
  uint16_t chasePeriodMs=900;
  bool chaseNeighbors=true, chaseInvert=false;
  uint32_t autoResetMs=0; // Opt-in; zero disables automatic reset.
  uint8_t winnerCue=BoardCore::Protocol::kCueWinner, resetCue=BoardCore::Protocol::kCueWelcome;
  uint32_t idleRgb=0xffffff, winRgb=0x00ff00, loseRgb=0xff0000;
  bool patch(const uint8_t *p,uint8_t n) {
    if((n!=17 && n!=19) || !p[0] || (n==17 && (p[0]&192U)))return false;
    KeyGameSettings next=*this;
    if(p[0]&1)next.enabledMask=p[1]|uint16_t(p[2])<<8;
    uint32_t *colors[]={&next.idleRgb,&next.winRgb,&next.loseRgb};
    for(uint8_t i=0;i<3;++i)if(p[0]&(2U<<i)) {
      const uint8_t at=3+i*3;*colors[i]=uint32_t(p[at])|uint32_t(p[at+1])<<8|uint32_t(p[at+2])<<16;
    }
    if(p[0]&16){if(p[12]>1)return false;next.extendedEffects=p[12]!=0;}
    if(p[0]&32)next.autoResetMs=uint32_t(p[13])|uint32_t(p[14])<<8|uint32_t(p[15])<<16|uint32_t(p[16])<<24;
    if(p[0]&64)next.winnerCue=p[17];
    if(p[0]&128)next.resetCue=p[18];
    if(!BoardCore::AudioCues::validGameCue(next.winnerCue)||!BoardCore::AudioCues::validGameCue(next.resetCue))return false;
    if(!next.enabledMask || next.enabledMask>511 || next.autoResetMs>86400000UL)return false;
    *this=next;return true;
  }
  bool enabled(uint8_t key)const{return key>=1&&key<=9&&(enabledMask&(1U<<(key-1)));}
  uint8_t count()const{uint8_t n=0;for(uint8_t k=1;k<=9;++k)if(enabled(k))++n;return n;}
  uint16_t spanPixels()const{for(uint8_t k=9;k;--k)if(enabled(k))return k*24;return 0;}
  String json()const{
    JsonDocument d;d["enabled_mask"]=enabledMask;d["enabled_count"]=count();
    presentationJson(d["presentation"].to<JsonObject>());
    d["active_pixels"]=count()*24;d["span_pixels"]=spanPixels();d["capacity_pixels"]=216;
    d["idle_rgb"]=idleRgb;d["win_rgb"]=winRgb;d["lose_rgb"]=loseRgb;
    d["extended_effects"]=extendedEffects;
    d["chase_period_ms"]=chasePeriodMs;
    d["chase_neighbors"]=chaseNeighbors;d["chase_invert"]=chaseInvert;
    d["auto_reset_ms"]=autoResetMs;
    d["winner_cue"]=BoardCore::AudioCues::gameName(winnerCue);
    d["reset_cue"]=BoardCore::AudioCues::gameName(resetCue);
    String s;serializeJson(d,s);return s;
  }
  bool load(fs::FS &fs){
    auto f=fs.open("/keys.json","r");if(!f||f.size()>8192)return false;
    JsonDocument d;if(deserializeJson(d,f))return false;
    if(!d["enabled_mask"].is<unsigned>()||!d["idle_rgb"].is<uint32_t>()||
       !d["win_rgb"].is<uint32_t>()||!d["lose_rgb"].is<uint32_t>())return false;
    const unsigned mask=d["enabled_mask"];
    if(!mask||mask>511||d["idle_rgb"].as<uint32_t>()>0xffffff||
       d["win_rgb"].as<uint32_t>()>0xffffff||d["lose_rgb"].as<uint32_t>()>0xffffff)return false;
    if(!d["extended_effects"].isNull()&&!d["extended_effects"].is<bool>())return false;
    if(!d["chase_period_ms"].isNull() && (!d["chase_period_ms"].is<uint16_t>() || d["chase_period_ms"].as<uint16_t>()<400 || d["chase_period_ms"].as<uint16_t>()>3000))return false;
    if(!d["chase_neighbors"].isNull()&&!d["chase_neighbors"].is<bool>())return false;
    if(!d["chase_invert"].isNull()&&!d["chase_invert"].is<bool>())return false;
    if(!d["auto_reset_ms"].isNull() && (!d["auto_reset_ms"].is<uint32_t>() || d["auto_reset_ms"].as<uint32_t>()>86400000UL))return false;
    uint8_t loadedWinner=BoardCore::Protocol::kCueWinner,loadedReset=BoardCore::Protocol::kCueWelcome;
    if(!d["winner_cue"].isNull() && (!d["winner_cue"].is<const char *>() || !BoardCore::AudioCues::parseGame(d["winner_cue"],loadedWinner)))return false;
    if(!d["reset_cue"].isNull() && (!d["reset_cue"].is<const char *>() || !BoardCore::AudioCues::parseGame(d["reset_cue"],loadedReset)))return false;
    if(!d["presentation"].isNull()&&!patchPresentation(d["presentation"]))return false;
    winnerCue=loadedWinner;resetCue=loadedReset;
    extendedEffects=d["extended_effects"]|false;
    chasePeriodMs=d["chase_period_ms"]|uint16_t(900);
    chaseNeighbors=d["chase_neighbors"]|true;
    chaseInvert=d["chase_invert"]|false;
    autoResetMs=d["auto_reset_ms"]|0U;
    enabledMask=mask;idleRgb=d["idle_rgb"];winRgb=d["win_rgb"];loseRgb=d["lose_rgb"];return true;
  }
  bool save(fs::FS &fs)const{
    const String data=json();auto old=fs.open("/keys.json","r");
    if(old&&old.readString()==data)return true;old.close();
    auto f=fs.open("/keys.pending","w");if(!f)return false;
    const bool ok=f.print(data)==data.length();f.close();if(!ok)return false;
    f=fs.open("/keys.pending","r");const bool exact=f&&f.readString()==data;f.close();
    return exact&&fs.rename("/keys.pending","/keys.json");
  }
};
