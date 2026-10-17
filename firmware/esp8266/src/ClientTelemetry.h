#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Bounded authenticated operational reports, not remote access to a PC.
// Incoming IP is observed by ESP; other client fields are self-reported.
class ClientTelemetry {
 public:
  bool report(const String &json,const String &peer,uint32_t now) {
    if(json.length()>1024) return false;
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
    const char *fields[]={"uptime_seconds","process_id","cpu_count","memory_mb","screen_width","screen_height","memory_available_mb","monitor_count"};
    for(unsigned i=0;i<8;++i) if(!d[fields[i]].isNull()) {
      if(!d[fields[i]].is<uint32_t>()) return false;
      next.numbers[i]=d[fields[i]].as<uint32_t>();next.present|=1U<<i;
    }
    peer.toCharArray(next.peer,sizeof(next.peer));next.last=now;next.active=true;
    for(auto &e:entries_) if(e.active && !strcmp(e.id,next.id)) {
      // A different authenticated peer may not overwrite another peer's ID.
      if(strcmp(e.peer,next.peer) && static_cast<uint32_t>(now-e.last)<120000U) return false;
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
    char id[49]={},hostname[65]={},os[33]={},arch[17]={},app[33]={},version[17]={},transport[17]={},peer[16]={};
    uint32_t numbers[8]={},last=0;uint8_t present=0;bool active=false;
  } entries_[8];
  static void populate(JsonObject o,const Entry &e,uint32_t now) {
      o["id"]=e.id;o["hostname"]=e.hostname;o["os"]=e.os;o["architecture"]=e.arch;
      o["application"]=e.app;o["version"]=e.version;o["transport"]=e.transport;
      o["observed_ip"]=e.peer;o["self_reported"]=true;o["age_ms"]=static_cast<uint32_t>(now-e.last);
      o["recent"]=static_cast<uint32_t>(now-e.last)<120000U;
      const char *fields[]={"uptime_seconds","process_id","cpu_count","memory_mb","screen_width","screen_height","memory_available_mb","monitor_count"};
      for(unsigned i=0;i<8;++i) if(e.present&(1U<<i)) o[fields[i]]=e.numbers[i];
      else o[fields[i]]=nullptr;
  }
  static bool text(JsonVariantConst v,char *out,size_t capacity,bool required=false) {
    if(v.isNull()) return !required;
    if(!v.is<const char*>()) return false;
    const char *s=v.as<const char*>();const size_t n=strlen(s);
    if(n>=capacity || (required&&!n)) return false;
    for(size_t i=0;i<n;++i) if(static_cast<uint8_t>(s[i])<32 || static_cast<uint8_t>(s[i])>126) return false;
    memcpy(out,s,n+1);return true;
  }
};
