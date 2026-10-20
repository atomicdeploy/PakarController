#pragma once
#include <ArduinoJson.h>
#include <FS.h>
// ESP owns persistent game configuration. AVR receives the enabled mask over
// the existing acknowledged link; it must not guess configuration after reset.
// Retains SettingsStore's unchanged-write avoidance and SceneStore's
// stage/readback/rename commit (never truncate the last committed settings).
struct KeyGameSettings {
  uint16_t enabledMask=511;
  uint32_t idleRgb=0xffffff, winRgb=0x00ff00, loseRgb=0xff0000;
  bool enabled(uint8_t key)const{return key>=1&&key<=9&&(enabledMask&(1U<<(key-1)));}
  uint8_t count()const{uint8_t n=0;for(uint8_t k=1;k<=9;++k)if(enabled(k))++n;return n;}
  uint16_t spanPixels()const{for(uint8_t k=9;k;--k)if(enabled(k))return k*24;return 0;}
  String json()const{
    JsonDocument d;d["enabled_mask"]=enabledMask;d["enabled_count"]=count();
    d["active_pixels"]=count()*24;d["span_pixels"]=spanPixels();d["capacity_pixels"]=216;
    d["idle_rgb"]=idleRgb;d["win_rgb"]=winRgb;d["lose_rgb"]=loseRgb;
    String s;serializeJson(d,s);return s;
  }
  bool load(fs::FS &fs){
    auto f=fs.open("/keys.json","r");if(!f||f.size()>512)return false;
    JsonDocument d;if(deserializeJson(d,f))return false;
    if(!d["enabled_mask"].is<unsigned>()||!d["idle_rgb"].is<uint32_t>()||
       !d["win_rgb"].is<uint32_t>()||!d["lose_rgb"].is<uint32_t>())return false;
    const unsigned mask=d["enabled_mask"];
    if(!mask||mask>511||d["idle_rgb"].as<uint32_t>()>0xffffff||
       d["win_rgb"].as<uint32_t>()>0xffffff||d["lose_rgb"].as<uint32_t>()>0xffffff)return false;
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
