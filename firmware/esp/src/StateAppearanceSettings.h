#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <StateAppearance.h>

// Same changed-byte avoidance and stage/readback/rename as KeyGameSettings.
// A palette is saved independently from semantic assignments and previews.
struct StateAppearanceSettings {
  BoardCore::StateAppearance value;
  bool patch(JsonVariantConst input) {
    if(!input.is<JsonObjectConst>())return false;
    auto next=value;
    for(JsonPairConst item:input.as<JsonObjectConst>()){
      const String key=item.key().c_str();const auto v=item.value();
      if(key=="enabled"||key=="neighbors"||key=="invert"||key=="diagonal"){
        if(!v.is<bool>())return false;
        if(key=="enabled")next.enabled=v.as<bool>();
        else if(key=="neighbors")next.neighbors=v.as<bool>();
        else if(key=="diagonal")next.diagonal=v.as<bool>();else next.invert=v.as<bool>();
      }else if(key=="pulse_ms"||key=="ring_ms"||key=="chase_ms"||key=="fade_ms"){
        if(!v.is<uint16_t>())return false;
        if(key=="pulse_ms")next.pulseMs=v.as<uint16_t>();
        else if(key=="ring_ms")next.ringMs=v.as<uint16_t>();
        else if(key=="chase_ms")next.chaseMs=v.as<uint16_t>();else next.fadeMs=v.as<uint16_t>();
      }else if(key=="states"){
        if(!v.is<JsonObjectConst>())return false;
        uint8_t seen=0;
        for(JsonPairConst state:v.as<JsonObjectConst>()){
          BoardCore::Protocol::CellState id;
          if(!BoardCore::StateAppearance::parse(state.key().c_str(),id)||
              (seen&(1U<<id))||!state.value().is<JsonObjectConst>())return false;
          seen|=1U<<id;
          for(JsonPairConst property:state.value().as<JsonObjectConst>()){
            const String name=property.key().c_str();
            if(name=="rgb"){
              if(!property.value().is<uint32_t>()||property.value().as<uint32_t>()>0xffffff)return false;
              next.states[id].rgb=property.value().as<uint32_t>();
            }else if(name=="motion"){
              if(!property.value().is<uint8_t>()||property.value().as<uint8_t>()>3)return false;
              next.states[id].motion=property.value().as<uint8_t>();
            }else return false;
          }
        }
      }else return false;
    }
    if(!next.valid())return false;
    value=next;return true;
  }
  String json()const{
    JsonDocument d;d["enabled"]=value.enabled;d["neighbors"]=value.neighbors;d["invert"]=value.invert;d["diagonal"]=value.diagonal;
    d["pulse_ms"]=value.pulseMs;d["ring_ms"]=value.ringMs;d["chase_ms"]=value.chaseMs;d["fade_ms"]=value.fadeMs;
    for(uint8_t i=0;i<4;++i){auto s=d["states"][BoardCore::StateAppearance::name(i)].to<JsonObject>();
      s["rgb"]=value.states[i].rgb;s["motion"]=value.states[i].motion;}
    String out;serializeJson(d,out);return out;
  }
  bool load(fs::FS &fs){
    auto f=fs.open("/presentation.json","r");if(!f||f.size()>2048)return false;
    JsonDocument d;if(deserializeJson(d,f))return false;return patch(d.as<JsonVariantConst>());
  }
  bool save(fs::FS &fs)const{
    const String data=json();auto old=fs.open("/presentation.json","r");
    if(old&&old.readString()==data)return true;old.close();
    auto f=fs.open("/presentation.pending","w");if(!f)return false;
    const bool written=f.print(data)==data.length();f.close();if(!written)return false;
    f=fs.open("/presentation.pending","r");const bool exact=f&&f.readString()==data;f.close();
    return exact&&fs.rename("/presentation.pending","/presentation.json");
  }
};
