#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include "NetworkObservationPolicy.h"

class NetworkToolsSettings {
 public:
  bool ntpEnabled=true,ntpDhcp=true;
  String ntpServer="pool.ntp.org",dateUrl="https://example.com/",publicIpUrl="https://api.ipify.org/";
  // No unsolicited calls to third-party lookup services. NTP remains automatic.
  bool dateAutomatic=false,publicIpAutomatic=false;
  uint32_t refreshSeconds=3600;
  void begin(fs::FS &fs,bool mounted){
    fs_=mounted?&fs:nullptr;if(!fs_)return;
    auto f=fs_->open("/network-tools.json","r");if(!f||f.size()>1536)return;
    JsonDocument d;if(deserializeJson(d,f)||!d.is<JsonObject>()||d.size()!=8)return;
    NetworkToolsSettings next=*this;String error;
    if(next.patch(d.as<JsonObjectConst>(),error))*this=next;
  }
  void append(JsonObject d)const {
    d["ntp_enabled"]=ntpEnabled;d["ntp_dhcp"]=ntpDhcp;d["ntp_server"]=ntpServer;
    d["http_date_url"]=dateUrl;d["http_date_automatic"]=dateAutomatic;
    d["public_ip_url"]=publicIpUrl;d["public_ip_automatic"]=publicIpAutomatic;
    d["refresh_seconds"]=refreshSeconds;
  }
  bool patch(JsonObjectConst d,String &error){
    if(d.isNull()||!d.size()||d.size()>8){error="invalid_network_tools_settings";return false;}
    for(auto field:d){const String key=field.key().c_str();const auto v=field.value();
      bool *flag=key=="ntp_enabled"?&ntpEnabled:key=="ntp_dhcp"?&ntpDhcp:
          key=="http_date_automatic"?&dateAutomatic:key=="public_ip_automatic"?&publicIpAutomatic:nullptr;
      if(flag){if(!v.is<bool>()){error="boolean_required";return false;}*flag=v.as<bool>();continue;}
      if(key=="refresh_seconds"){
        if(!v.is<uint32_t>()||v.as<uint32_t>()<60||v.as<uint32_t>()>86400){error="refresh_seconds_60_to_86400";return false;}
        refreshSeconds=v.as<uint32_t>();continue;
      }
      String *text=key=="ntp_server"?&ntpServer:key=="http_date_url"?&dateUrl:key=="public_ip_url"?&publicIpUrl:nullptr;
      if(!text||!v.is<const char*>()){error="unknown_or_invalid_setting";return false;}
      const JsonString raw=v.as<JsonString>();
      if(!raw.size()||raw.size()>192||memchr(raw.c_str(),0,raw.size())){error="invalid_setting_text";return false;}
      *text=v.as<String>();
      if(key=="ntp_server"?!PakarNetworkObservation::hostname(text->c_str()):!url(*text)){
        error="invalid_hostname_or_https_url";return false;
      }
    }
    return true;
  }
  bool save(String &error){
    if(!fs_){error="filesystem_unavailable";return false;}
    JsonDocument d;append(d.to<JsonObject>());String raw;serializeJson(d,raw);
    if(d.overflowed()||raw.length()!=measureJson(d)){error="settings_memory_unavailable";return false;}
    auto old=fs_->open("/network-tools.json","r");const bool same=old&&old.readString()==raw;old.close();
    if(same)return true;
    auto f=fs_->open("/network-tools.pending","w");if(!f){error="settings_write_failed";return false;}
    const bool wrote=f.print(raw)==raw.length();f.close();
    f=fs_->open("/network-tools.pending","r");const bool exact=f&&f.readString()==raw;f.close();
    if(!wrote||!exact||!fs_->rename("/network-tools.pending","/network-tools.json")){error="settings_publish_failed";return false;}
    return true;
  }
  static bool url(const String &s){
    if(!s.startsWith("https://")||s.length()>192)return false;
    for(unsigned i=0;i<s.length();++i)if(uint8_t(s[i])<=32||s[i]==127||s[i]=='@'||s[i]=='#'||s[i]=='\\')return false;
    const int slash=s.indexOf('/',8);const String host=slash<0?s.substring(8):s.substring(8,slash);
    return PakarNetworkObservation::hostname(host.c_str());
  }
 private:fs::FS *fs_=nullptr;
};
