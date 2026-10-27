#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PeerRoutePolicy.h>
#include <PeerCompatibility.h>
#include <BuildInfo.h>
#include <mutex>
#include <atomic>

// One bounded outgoing job, never network I/O on the game/HTTP owner. Explicit
// target board+boot fencing, no redirects/retries/credential forwarding. The
// target retains ownership of command expiry, deduplication and actual ACKs.
class PeerRelay {
 public:
  bool busy()const{return busy_.load();}
  String submit(const String &raw,const String &self) {
    using namespace BoardCore::PeerRoutePolicy;
    std::lock_guard<std::mutex> lock(mutex_);
    if(raw.length()>4096)return error("request_too_large");
    JsonDocument d;if(deserializeJson(d,raw)||!d.is<JsonObject>())return error("invalid_request");
    const String id=d["request_id"]|"",host=d["hostname"]|"",board=d["board_id"]|"",boot=d["boot_id"]|"",path=d["path"]|"",method=d["method"]|"";
    const bool bootstrap=method=="GET"&&path=="/api/peer/info"&&boot.isEmpty();
    if(!hexId(id.c_str())||!hostname(host.c_str())||!boardId(board.c_str())||(!bootstrap&&!hexId(boot.c_str()))||board==self)return error("invalid_peer_identity");
    const bool command=method=="POST"&&path=="/api/command";
    if(!(method=="GET"&&readPath(path.c_str()))&&!command)return error("unsupported_peer_route");
    const String packet=d["packet"]|"";
    if(command) {
      JsonDocument p;
      if(packet.length()>3072||deserializeJson(p,packet)||p["type"]!="command"||p["request_id"]!=id||p["board_id"]!=board||p["boot_id"]!=boot||!p["expires_at_ms"].is<uint64_t>())return error("invalid_command_envelope");
    } else if(packet.length())return error("unexpected_packet");
    for(const auto &old:history_)if(old.id==id&&uint32_t(millis()-old.at)<60000U) {
      if(old.raw!=raw)return error("request_id_conflict");
      if(id==id_)return snapshotLocked();
      return error("receipt_retired_do_not_replay");
    }
    if(busy())return error("peer_relay_busy");
    History *slot=nullptr;for(auto &h:history_)if(!h.id.length()||uint32_t(millis()-h.at)>=60000U){slot=&h;break;}
    if(!slot)return error("peer_receipts_full");
    if(ESP.getFreeHeap()<70000)return error("peer_relay_low_memory");
    *slot={id,raw,millis()};id_=id;host_=host;board_=board;boot_=boot;path_=path;packet_=packet;self_=self;
    command_=command;response_="";failure_="";code_=0;phase_="queued";verified_=false;started_=millis();
    busy_=true;
    if(xTaskCreate(worker,"pakar-peer",10240,this,1,nullptr)!=pdPASS){busy_=false;phase_="failed";failure_="worker_unavailable";}
    return snapshotLocked();
  }
  String snapshot(const String &id=String()) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(id.length()&&id!=id_)return error("unknown_peer_request");
    return snapshotLocked();
  }
 private:
  struct History {String id,raw;uint32_t at=0;} history_[8];
  std::mutex mutex_;std::atomic<bool> busy_{false};
  String id_,host_,board_,boot_,path_,packet_,self_,response_,failure_,phase_="idle";
  bool command_=false,verified_=false;int code_=0;uint32_t started_=0;
  static String error(const char *e){JsonDocument d;d["accepted"]=false;d["error"]=e;String s;serializeJson(d,s);return s;}
  String snapshotLocked() {
    JsonDocument d;d["request_id"]=id_;d["phase"]=phase_;d["busy"]=busy();d["target_board_id"]=board_;d["target_boot_id"]=boot_;
    d["hostname"]=host_;d["via_board_id"]=self_;d["transport"]="http-lan";d["hop_count"]=1;d["automatic_retry"]=false;
    d["response_verified"]=verified_;d["http_status"]=code_;d["error"]=failure_;d["response"]=response_;
    d["accepted"]=id_.length()>0&&failure_.isEmpty();String s;serializeJson(d,s);return s;
  }
  static void worker(void *p){auto &s=*static_cast<PeerRelay*>(p);s.run();s.busy_=false;vTaskDelete(nullptr);}
  String request(const IPAddress &ip,const char *path,bool post,int &code,bool &fenced) {
    NetworkClient client;HTTPClient http;String result;
    http.setConnectTimeout(1000);http.setTimeout(1000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.setReuse(false);
    const String url=String("http://")+ip.toString()+path;
    if(!http.begin(client,url))return String();
    http.addHeader("Host",host_);http.addHeader("User-Agent",String("PakarController/")+BoardCore::BuildInfo::kVersion+" (peer-relay)");
    http.addHeader("X-Pakar-Version",BoardCore::BuildInfo::kVersion);
    http.addHeader("X-Pakar-Board",board_);if(boot_.length())http.addHeader("X-Pakar-Boot",boot_);
    http.addHeader("X-Pakar-Via",self_); // Metadata only, never downstream authority.
    const char *headers[]={"X-Pakar-Board","X-Pakar-Boot"};http.collectHeaders(headers,2);
    if(post) {
      String form="packet=";const char hex[]="0123456789ABCDEF";
      for(size_t i=0;i<packet_.length();++i){uint8_t c=packet_[i];form+='%';form+=hex[c>>4];form+=hex[c&15];}
      http.addHeader("Content-Type","application/x-www-form-urlencoded");code=http.POST(form);
    }else code=http.GET();
    fenced=http.header("X-Pakar-Board")==board_&&(boot_.length()?http.header("X-Pakar-Boot")==boot_:BoardCore::PeerRoutePolicy::hexId(http.header("X-Pakar-Boot").c_str()));
    const int size=http.getSize();
    if(code==200&&fenced&&size>0&&size<=24576) {
      result.reserve(size);auto *stream=http.getStreamPtr();uint8_t block[512];
      while(result.length()<static_cast<size_t>(size)&&uint32_t(millis()-started_)<6000U) {
        const size_t available=stream->available();
        if(!available){if(!http.connected())break;vTaskDelay(1);continue;}
        const size_t count=min<size_t>(sizeof(block),min<size_t>(available,size-result.length()));
        const int n=stream->read(block,count);if(n<=0)break;
        result.concat(reinterpret_cast<const char*>(block),n);
      }
      if(result.length()!=static_cast<size_t>(size))result="";
    }
    http.end();return result;
  }
  void run() {
    IPAddress ip;String errorText,body;bool fenced=false,verified=false;int code=0;
    if(!WiFi.hostByName(host_.c_str(),ip)||!BoardCore::PeerRoutePolicy::privateAddress(ip[0],ip[1],ip[2],ip[3])||ip==WiFi.localIP()||ip==WiFi.softAPIP())errorText="peer_lan_resolution_failed";
    else {
      body=request(ip,"/api/peer/info",false,code,fenced);JsonDocument identity;
      if(!fenced||deserializeJson(identity,body)||identity["application"]!="PakarController"||identity["board_id"]!=board_||
          !BoardCore::PeerRoutePolicy::hexId(identity["boot_id"]|"")||(boot_.length()&&identity["boot_id"]!=boot_)||!BoardCore::PeerCompatibility::boardAllowed(identity["version"]|""))errorText="peer_identity_not_verified";
      else if(path_=="/api/peer/info")verified=true;
      else if(uint32_t(millis()-started_)>=4000U)errorText="peer_deadline_elapsed";
      else {
        body=request(ip,path_.c_str(),command_,code,fenced);JsonDocument reply;
        if(!fenced||body.isEmpty()||deserializeJson(reply,body)||!reply.is<JsonObject>())errorText=command_?"command_outcome_uncertain_do_not_replay":"peer_response_unavailable";
        else if(command_&&(reply["type"]!="command-result"||reply["request_id"]!=id_||reply["board_id"]!=board_||reply["boot_id"]!=boot_||!reply["accepted"].is<bool>()))errorText="uncorrelated_peer_receipt";
        else verified=true; // Transport receipt, NOT physical success/command acceptance.
      }
    }
    std::lock_guard<std::mutex> lock(mutex_);response_=verified?body:String();failure_=errorText;code_=code;verified_=verified;phase_=verified?"complete":"failed";
  }
};
#endif
