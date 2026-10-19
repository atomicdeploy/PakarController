#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PeerCompatibility.h>

// Bounded authenticated operational reports, not remote access to a PC.
// Incoming IP is observed by ESP; other client fields are self-reported.
class ClientTelemetry {
 public:
  __attribute__((noinline)) bool report(const String &json,const String &peer,uint32_t now,const String &agent=String(),const String &headerVersion=String()) {
    if(json.length()>1536) return false;
    JsonDocument d;
    if(deserializeJson(d,json) || !d.is<JsonObject>()) return false;
    Entry next;
    if(!text(d["id"],next.id,sizeof(next.id),true) ||
       !text(d["hostname"],next.hostname,sizeof(next.hostname)) ||
       !text(d["os"],next.os,sizeof(next.os)) ||
       !text(d["architecture"],next.arch,sizeof(next.arch)) ||
       !text(d["application"],next.app,sizeof(next.app)) ||
       !text(d["version"],next.version,sizeof(next.version)) ||
       !text(d["transport"],next.transport,sizeof(next.transport))) return false;
    if(!BoardCore::PeerCompatibility::appAllowed(next.version) ||
       (headerVersion.length() && headerVersion!=next.version))return false;
    // Preserve only public operational summaries; never retain arbitrary
    // supplied settings, credentials, file inventories or network secrets.
    JsonDocument extra;
    const char *texts[]={"session_id","device_model","reported_ip"};
    for(const char *key:texts) if(!d[key].isNull()) {
      char value[97]{};if(!text(d[key],value,sizeof(value)))return false;extra[key]=value;
    }
    if(!d["build"].isNull()) {
      if(d["build"].is<uint32_t>())extra["build"]=d["build"].as<uint32_t>();
      else {char value[33]{};if(!text(d["build"],value,sizeof(value)))return false;extra["build"]=value;}
    }
    if(!d["capabilities"].isNull()) {
      if(!d["capabilities"].is<JsonArray>() || d["capabilities"].size()>12)return false;
      auto caps=extra["capabilities"].to<JsonArray>();
      for(JsonVariantConst item:d["capabilities"].as<JsonArrayConst>()) {
        char value[33]{};if(!text(item,value,sizeof(value),true))return false;
        bool found=false;for(JsonVariantConst known:caps)if(!strcmp(known.as<const char*>(),value))found=true;
        if(!found)caps.add(value);
      }
    }
    const char *hardware[]={"model","manufacturer","cpu","os_version","sdk","memory_mb","cpu_count","screen_width","screen_height"};
    const char *settings[]={"transport","theme","language","direction","palette","sound","silent_mode","telemetry_enabled","version_check"};
    if((!d["hardware"].isNull()&&!summary(d["hardware"],extra["hardware"].to<JsonObject>(),hardware,sizeof(hardware)/sizeof(*hardware))) ||
       (!d["settings"].isNull()&&!summary(d["settings"],extra["settings"].to<JsonObject>(),settings,sizeof(settings)/sizeof(*settings))))return false;
    String encoded;if(serializeJson(extra,encoded)!=measureJson(extra) || encoded.length()>=sizeof(next.extra))return false;
    encoded.toCharArray(next.extra,sizeof(next.extra));
    for(size_t i=0;i<agent.length();++i)if(static_cast<uint8_t>(agent[i])<32 || static_cast<uint8_t>(agent[i])>126)return false;
    next.agentTruncated=agent.length()>=sizeof(next.agent);
    agent.toCharArray(next.agent,sizeof(next.agent));
    const char *fields[]={"uptime_seconds","process_id","cpu_count","memory_mb","screen_width","screen_height","memory_available_mb","monitor_count"};
    for(unsigned i=0;i<8;++i) if(!d[fields[i]].isNull()) {
      if(!d[fields[i]].is<uint32_t>()) return false;
      next.numbers[i]=d[fields[i]].as<uint32_t>();next.present|=1U<<i;
    }
    peer.toCharArray(next.peer,sizeof(next.peer));next.last=now;next.active=true;
    for(auto &e:entries_) if(e.active && !strcmp(e.id,next.id)) {
      // A different authenticated peer may not overwrite another peer's ID.
      if(strcmp(e.peer,next.peer) && static_cast<uint32_t>(now-e.last)<120000U) {
        JsonDocument previous;
        if(deserializeJson(previous,e.extra) || !d["session_id"].is<const char*>() ||
           !strlen(d["session_id"].as<const char*>()) ||
           previous["session_id"]!=d["session_id"] || strcmp(e.app,next.app) || strcmp(e.version,next.version))return false;
      }
      e=next;return true;
    }
    for(auto &e:entries_) if(!e.active || static_cast<uint32_t>(now-e.last)>=120000U) {
      e=next;return true;
    }
    return false;
  }
  void append(JsonArray out,uint32_t now) const {
    for(const auto &e:entries_) if(e.active) {
      auto o=out.add<JsonObject>();
      populate(o,e,now);
    }
  }
  // Serialize one client at a time rather than retaining eight JSON trees
  // beside the full ESP/AVR document on this memory-constrained ESP8266.
  String json(uint32_t now) const {
    String out="[";bool first=true;
    for(const auto &e:entries_) if(e.active) {
      JsonDocument d;populate(d.to<JsonObject>(),e,now);
      if(d.overflowed())return String();
      // ArduinoJson's ArduinoStringWriter clears its destination. Serialize
      // this one bounded entry separately, then explicitly append it; using
      // serializeJson(d,out) would erase the array prefix/previous clients.
      String entry;const size_t expected=measureJson(d);
      if(serializeJson(d,entry)!=expected || entry.length()!=expected)return String();
      size_t before=out.length();
      if(!first){out+=',';if(out.length()!=before+1)return String();++before;}
      first=false;out+=entry;
      if(out.length()!=before+expected)return String();
    }
    const size_t before=out.length();out+=']';
    return out.length()==before+1?out:String();
  }
 private:
  struct Entry {
    char id[49]={},hostname[65]={},os[33]={},arch[17]={},app[33]={},version[64]={},transport[17]={},peer[46]={};
    char extra[769]={},agent[193]={};
    uint32_t numbers[8]={},last=0;uint8_t present=0;bool active=false,agentTruncated=false;
  } entries_[8];
  static void populate(JsonObject o,const Entry &e,uint32_t now) {
      o["id"]=e.id;o["hostname"]=e.hostname;o["os"]=e.os;o["architecture"]=e.arch;
      o["application"]=e.app;o["version"]=e.version;o["transport"]=e.transport;
      o["user_agent"]=e.agent;
      o["user_agent_truncated"]=e.agentTruncated;
      JsonDocument extra;
      if(!deserializeJson(extra,e.extra))for(JsonPairConst value:extra.as<JsonObjectConst>())o[value.key()]=value.value();
      // Local transport markers are not IP addresses. Keep the receiver's
      // observation separate from the caller's claimed route/address.
      const bool usb=!strcmp(e.peer,"usb"),ble=!strcmp(e.peer,"ble");
      o["observed_transport"]=usb?"usb":ble?"ble":"network";
      if(usb||ble)o["observed_ip"]=nullptr;
      else o["observed_ip"]=e.peer;
      o["self_reported"]=true;o["age_ms"]=static_cast<uint32_t>(now-e.last);
      o["recent"]=static_cast<uint32_t>(now-e.last)<120000U;
      const char *fields[]={"uptime_seconds","process_id","cpu_count","memory_mb","screen_width","screen_height","memory_available_mb","monitor_count"};
      for(unsigned i=0;i<8;++i) if(e.present&(1U<<i)) o[fields[i]]=e.numbers[i];
      else o[fields[i]]=nullptr;
  }
  static bool summary(JsonVariantConst source,JsonObject target,const char *const *keys,size_t count) {
    if(source.isNull())return true;
    if(!source.is<JsonObjectConst>())return false;
    auto object=target;
    for(size_t i=0;i<count;++i) {
      const auto value=source[keys[i]];if(value.isNull())continue;
      if(value.is<bool>())object[keys[i]]=value.as<bool>();
      else if(value.is<uint32_t>())object[keys[i]]=value.as<uint32_t>();
      else {char buffer[97]{};if(!text(value,buffer,sizeof(buffer)))return false;object[keys[i]]=buffer;}
    }
    return true;
  }
  static bool text(JsonVariantConst v,char *out,size_t capacity,bool required=false) {
    if(v.isNull()) return !required;
    if(!v.is<const char*>()) return false;
    const char *s=v.as<const char*>();const size_t n=strlen(s);
    if(n>=capacity || (required&&!n)) return false;
    // Public device/model/hostname text may be Unicode. Validate bounded UTF-8
    // and reject controls/overlong encodings, rather than dropping real names.
    for(size_t i=0;i<n;) {
      const uint8_t c=static_cast<uint8_t>(s[i++]);
      if(c<128){if(c<32||c==127)return false;continue;}
      unsigned rest=0;uint32_t point=0,minimum=0;
      if(c>=0xc2&&c<=0xdf){rest=1;point=c&31;minimum=0x80;}
      else if(c>=0xe0&&c<=0xef){rest=2;point=c&15;minimum=0x800;}
      else if(c>=0xf0&&c<=0xf4){rest=3;point=c&7;minimum=0x10000;}
      else return false;
      if(n-i<rest)return false;
      while(rest--){const uint8_t b=static_cast<uint8_t>(s[i++]);if((b&0xc0)!=0x80)return false;point=(point<<6)|(b&63);}
      if(point<minimum || point<0xa0 || point>0x10ffff || (point>=0xd800&&point<=0xdfff))return false;
    }
    memcpy(out,s,n+1);return true;
  }
};
