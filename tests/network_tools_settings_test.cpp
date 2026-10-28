#include "../firmware/esp/src/NetworkToolsSettings.h"
#include <cassert>
#include <iostream>
bool patch(NetworkToolsSettings &s,const char *raw){JsonDocument d;assert(!deserializeJson(d,raw));String error;return s.patch(d.as<JsonObjectConst>(),error);}
int main(){
  fs::FS fs;NetworkToolsSettings s;s.begin(fs,true);String error;
  assert(s.ntpEnabled&&s.ntpDhcp&&s.ntpServer=="pool.ntp.org"&&!s.dateAutomatic&&!s.publicIpAutomatic&&fs.writes==0);
  assert(patch(s,R"({"ntp_server":"time.example.org","ntp_dhcp":false,"refresh_seconds":120})"));
  assert(s.save(error)&&fs.writes==1);
  assert(s.save(error)&&fs.writes==1); // No redundant flash writes.
  NetworkToolsSettings restored;restored.begin(fs,true);
  assert(restored.ntpServer==s.ntpServer&&!restored.ntpDhcp&&restored.refreshSeconds==120);
  for(const char *bad:{R"({"ntp_enabled":"true"})",R"({"ntp_server":"user@host"})",
      R"({"http_date_url":"http://example.com/"})",R"({"public_ip_url":"https://user@host/"})",
      R"({"public_ip_url":"https://example.com/\u0000evil"})",R"({"public_ip_url":"https://example.com/#fragment"})",
      R"({"refresh_seconds":59})",R"({"refresh_seconds":86401})",R"({"refresh_seconds":60.5})",
      R"({"epoch":1793190896})",R"({"public_ip":"8.8.8.8"})",R"({})"}){
    NetworkToolsSettings candidate=restored;assert(!patch(candidate,bad));
  }
  NetworkToolsSettings candidate=restored;assert(patch(candidate,R"({"ntp_enabled":false})"));
  fs.failWrite=true;assert(!candidate.save(error));fs.failWrite=false;
  NetworkToolsSettings afterWrite;afterWrite.begin(fs,true);assert(afterWrite.ntpEnabled);
  fs.failRename=true;assert(!candidate.save(error));fs.failRename=false;
  NetworkToolsSettings afterRename;afterRename.begin(fs,true);assert(afterRename.ntpEnabled);
  NetworkToolsSettings unavailable;unavailable.begin(fs,false);assert(!unavailable.save(error));
  assert(NetworkToolsSettings::url("https://example.com/date?source=clock"));
  assert(!NetworkToolsSettings::url("https://example.com:443/"));
  std::cout<<"Actual network-tools settings: typed patches, protected observations, staged persistence and write/rename failure passed\n";
}
