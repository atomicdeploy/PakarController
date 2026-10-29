#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BuildInfo.h>
#include <PeerCompatibility.h>
#include <PeerRoutePolicy.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <mutex>

// Detached, explicitly admitted one-shot commissioning. Reuses PeerRelay's
// pinned dial + logical Host, bounded body, board/boot fencing and no redirect /
// mutation-replay policy (PCController netpolicy/http_url.go + peer_response.go).
// Root owns the exclusive maintenance lease and pauses cloud/normal Wi-Fi work
// until busy()==false. Hooks are NetworkConfig's transient station owner; they
// must not save or modify this board's normal network record.
class PeerNetworkCommission {
 public:
  struct Hooks {
    bool (*join)(const String&,const String&,const uint8_t*)=nullptr;
    bool (*joined)(const String&,const uint8_t*)=nullptr;
    bool (*restore)()=nullptr;
    bool (*restored)()=nullptr;
  };
  bool begin(fs::FS &filesystem,bool mounted,Hooks hooks) {
    filesystem_=mounted?&filesystem:nullptr;hooks_=hooks;
    if(!filesystem_)return false;
    File saved=filesystem_->open(path_,"r");
    if(saved&&(saved.size()==0||saved.size()>4096)){saved.close();filesystem_=nullptr;return false;}
    if(saved&&saved.size()>0&&saved.size()<=4096){
      const String raw=saved.readString();JsonDocument receipt;
      if(!deserializeJson(receipt,raw)&&receipt["request_id"].is<const char*>()&&
          BoardCore::PeerRoutePolicy::hexId(receipt["request_id"])) {
        receipt_=raw;id_=receipt["request_id"].as<String>();
        // Never resume an accepted/possibly dispatched mutation on boot.
        // Normal startup already applies this board's untouched saved network.
        if(receipt["busy"]==true){
          receipt["busy"]=false;receipt["phase"]="interrupted";
          receipt["error"]="interrupted_do_not_replay";receipt["restart_resumed"]=false;
          receipt["restore_requested"]=true;receipt["restore_ready"]=false;
          if(!store(receipt)){receipt_=String();serializeJson(receipt,receipt_);}
        }
      }else {saved.close();filesystem_=nullptr;return false;}
    }
    if(saved)saved.close();
    return true;
  }
  bool busy()const{return busy_.load();}
  // Called by the loop owner ONLY after accepted reply delivery / cloud
  // callback return and explicit suspension of cloud sockets. No Wi-Fi change
  // is scheduled merely by receiving the request.
  bool start() {
    std::lock_guard<std::mutex> guard(mutex_);
    if(!busy()||launched_)return false;
    launched_=true;xTaskNotifyGive(worker_);return true;
  }
  String snapshot(const String &requestId=String()) {
    std::lock_guard<std::mutex> guard(mutex_);
    if(requestId.length()&&requestId!=id_)return error("unknown_peer_request");
    return receipt_.length()?receipt_:String("{\"phase\":\"idle\",\"busy\":false}");
  }
  String submit(const String &raw,const String &self) {
    std::lock_guard<std::mutex> guard(mutex_);
    if(raw.length()>1536)return error("request_too_large");
    JsonDocument d;if(deserializeJson(d,raw)||!d.is<JsonObject>())return error("invalid_request");
    const String id=d["request_id"]|"",board=d["board_id"]|"",host=d["hostname"]|"";
    if(!BoardCore::PeerRoutePolicy::hexId(id.c_str()))return error("invalid_request_id");
    // Duplicate receipt lookup NEVER schedules another job, even after reboot.
    if(id==id_)return receipt_;
    if(busy())return error("peer_network_commission_busy");
    if(!filesystem_||!hooks_.join||!hooks_.joined||!hooks_.restore||!hooks_.restored)return error("commissioning_unavailable");
    JsonDocument previous;if(receipt_.length()&&deserializeJson(previous,receipt_))return error("receipt_storage_failed");
    const JsonArrayConst retired=previous["used_request_ids"].as<JsonArrayConst>();
    for(JsonVariantConst old:retired)if(old==id)return error("receipt_retired_do_not_replay");
    // Explicit bounded receipt budget. Never evict an old mutation's ID and
    // accidentally permit its POST to be replayed after another job/reboot.
    if(retired.size()>=16)return error("commissioning_receipts_full");
    const String role=d["role"]|"",ap=d["ap_ssid"]|"",apPass=d["ap_password"]|"";
    const String ssid=d["station_ssid"]|"",password=d["station_password"]|"",mac=d["ap_bssid"]|"";
    if(d["confirm"]!="configure-peer-network"||!BoardCore::PeerRoutePolicy::boardId(board.c_str())||
        board==self||!BoardCore::PeerRoutePolicy::hostname(host.c_str())||host.indexOf('.')>=0||
        (role!="first-wins"&&role!="table-selection")||!credential(ap,apPass)||!credential(ssid,password)||
        !parseBssid(mac,bssid_))return error("invalid_commissioning_target");
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<60000U||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<24576U)return error("commissioning_low_memory");
    // Boot-fenced role/hostname checks below remain mandatory even when this
    // expected ID was inferred by the caller from a prior AP observation.
    if(!worker_&&xTaskCreate(worker,"pakar-peer-net",8192,this,1,&worker_)!=pdPASS)return error("worker_unavailable");
    JsonDocument receipt;receipt["request_id"]=id;receipt["target_board_id"]=board;
    receipt["hostname"]=host;receipt["role"]=role;receipt["phase"]="queued";receipt["busy"]=true;
    receipt["automatic_retry"]=false;receipt["restart_resumed"]=false;
    receipt["mutation_dispatched"]=false;receipt["settings_verified"]=false;
    receipt["restore_requested"]=false;receipt["restore_ready"]=false;
    receipt["accepted"]=true;receipt["error"]="";
    auto used=receipt["used_request_ids"].to<JsonArray>();for(JsonVariantConst old:retired)used.add(old);used.add(id);
    if(!store(receipt))return error("receipt_storage_failed");
    id_=id;board_=board;host_=host;role_=role;ap_=ap;apPassword_=apPass;
    ssid_=ssid;password_=password;boot_="";started_=millis();launched_=false;busy_=true;
    return receipt_;
  }
 private:
  static constexpr const char *path_="/peer-commission.json",*pending_="/peer-commission.pending";
  fs::FS *filesystem_=nullptr;Hooks hooks_{};TaskHandle_t worker_=nullptr;
  std::mutex mutex_;std::atomic<bool> busy_{false};uint8_t bssid_[6]{};
  String id_,board_,host_,role_,ap_,apPassword_,ssid_,password_,boot_,receipt_;
  uint32_t started_=0;
  bool launched_=false;
  static bool credential(const String &ssid,const String &password) {
    return ssid.length()>0&&ssid.length()<=32&&ssid.length()==strlen(ssid.c_str())&&
      password.length()<=63&&password.length()==strlen(password.c_str())&&
      (!password.length()||password.length()>=8);
  }
  static bool parseBssid(const String &text,uint8_t out[6]) {
    if(text.length()!=17)return false;
    unsigned any=0;
    auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    for(unsigned i=0;i<6;++i){const int a=nibble(text[i*3]),b=nibble(text[i*3+1]);
      if(a<0||b<0||(i<5&&text[i*3+2]!=':'))return false;
      out[i]=uint8_t(a*16+b);any|=out[i];}
    return any&&!(out[0]&1U);
  }
  static String error(const char *text){JsonDocument d;d["accepted"]=false;d["error"]=text;String raw;serializeJson(d,raw);return raw;}
  bool store(const JsonDocument &d) {
    String raw;const size_t expected=measureJson(d);
    if(!filesystem_||d.overflowed()||expected>4096||!raw.reserve(expected)||serializeJson(d,raw)!=expected||raw.length()!=expected)return false;
    File output=filesystem_->open(pending_,"w");if(!output)return false;
    const bool written=output.print(raw)==raw.length();output.close();
    File check=filesystem_->open(pending_,"r");const bool verified=check&&check.size()==raw.length()&&check.readString()==raw;
    if(check)check.close();
    if(!written||!verified||!filesystem_->rename(pending_,path_))return false;
    receipt_=raw;return true;
  }
  bool phase(const char *name,const char *failure="",bool dispatched=false) {
    std::lock_guard<std::mutex> guard(mutex_);JsonDocument d;
    if(deserializeJson(d,receipt_))return false;
    d["phase"]=name;d["error"]=failure;d["elapsed_ms"]=uint32_t(millis()-started_);
    if(dispatched)d["mutation_dispatched"]=true;
    if(boot_.length())d["target_boot_id"]=boot_;
    return store(d);
  }
  class PinnedClient final:public NetworkClient {
   public:explicit PinnedClient(const IPAddress &ip):ip_(ip){}
    int connect(const char*,uint16_t port)override{return NetworkClient::connect(ip_,port,700);}
    int connect(const char*,uint16_t port,int32_t timeout)override{return NetworkClient::connect(ip_,port,timeout);}
   private:IPAddress ip_;
  };
  String request(const IPAddress &ip,const char *path,const String *form,int &code,bool &fenced) {
    PinnedClient client(ip);HTTPClient http;String result;code=0;fenced=false;
    http.setConnectTimeout(700);http.setTimeout(1000);http.setReuse(false);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if(!http.begin(client,String("http://")+host_+".local"+path))return result;
    http.setUserAgent(String("PakarController/")+BoardCore::BuildInfo::kVersion+" (peer-network-commission)");
    http.addHeader("X-Pakar-Version",BoardCore::BuildInfo::kVersion);http.addHeader("X-Pakar-Board",board_);
    if(boot_.length())http.addHeader("X-Pakar-Boot",boot_);
    const char *headers[]={"X-Pakar-Board","X-Pakar-Boot"};http.collectHeaders(headers,2);
    if(form){http.addHeader("Content-Type","application/x-www-form-urlencoded");code=http.POST(*form);}else code=http.GET();
    const String replyBoot=http.header("X-Pakar-Boot");
    fenced=http.header("X-Pakar-Board")==board_&&BoardCore::PeerRoutePolicy::hexId(replyBoot.c_str())&&(!boot_.length()||replyBoot==boot_);
    const int count=http.getSize();
    if(fenced&&count>0&&count<=24576&&result.reserve(count)){
      auto *stream=http.getStreamPtr();uint8_t block[256];const uint32_t at=millis();
      while(result.length()<static_cast<size_t>(count)&&uint32_t(millis()-at)<1500U){
        const size_t ready=stream->available();if(!ready){if(!http.connected())break;vTaskDelay(1);continue;}
        const int n=stream->read(block,min<size_t>(sizeof(block),min<size_t>(ready,count-result.length())));
        if(n<=0)break;
        result.concat(reinterpret_cast<const char*>(block),n);
      }
      if(result.length()!=static_cast<size_t>(count))result="";
    }
    http.end();return result;
  }
  static String escaped(const String &value){String out;const char hex[]="0123456789ABCDEF";
    if(!out.reserve(value.length()*3))return out;
    for(size_t i=0;i<value.length();++i){uint8_t c=value[i];out+='%';out+=hex[c>>4];out+=hex[c&15];}return out;}
  static void worker(void *context){auto &job=*static_cast<PeerNetworkCommission*>(context);for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);job.run();}}
  const char *commission(bool &dispatched,bool &verified) {
    if(!phase("joining"))return "receipt_storage_failed";
    if(!hooks_.join(ap_,apPassword_,bssid_))return "temporary_station_start_failed";
    const uint32_t joining=millis();
    while(!hooks_.joined(ap_,bssid_)){
      if(uint32_t(millis()-joining)>=20000U)return "target_ap_join_timeout";
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    const IPAddress address=WiFi.gatewayIP();const String targetIp=address.toString();
    if(!BoardCore::PeerRoutePolicy::privateIpv4(targetIp.c_str())||address==WiFi.localIP())return "invalid_peer_gateway";
    int code=0;bool fenced=false;String raw=request(address,"/api/peer/info",nullptr,code,fenced);JsonDocument d;
    if(code!=200||!fenced||deserializeJson(d,raw)||d["application"]!="PakarController"||
        d["board_id"]!=board_||d["hostname"]!=host_||!BoardCore::PeerRoutePolicy::hexId(d["boot_id"]|"")||
        !BoardCore::PeerCompatibility::boardAllowed(d["version"]|""))return "peer_identity_not_verified";
    boot_=d["boot_id"].as<String>();
    if(!phase("verifying"))return "receipt_storage_failed";
    // Probe the baked-in role separately: peer/info intentionally stays small.
    raw=request(address,"/api/status",nullptr,code,fenced);
    JsonDocument filter;filter["capabilities"]["role"]=true;filter["board_id"]=true;filter["boot_id"]=true;
    d.clear();if(code!=200||!fenced||deserializeJson(d,raw,DeserializationOption::Filter(filter))||d["capabilities"]["role"]!=role_)
      return "peer_role_not_verified";
    raw=request(address,"/api/network",nullptr,code,fenced);d.clear();
    if(code!=200||!fenced||raw.length()>4096||deserializeJson(d,raw)||!d["station_enabled"].is<bool>()||!d["ap_enabled"].is<bool>())
      return code==401||code==403?"peer_authentication_required":"peer_network_snapshot_unavailable";
    {
      std::lock_guard<std::mutex> guard(mutex_);JsonDocument receipt;if(deserializeJson(receipt,receipt_))return "receipt_storage_failed";
      // Whitelist redacted policy fields; never persist arbitrary reply fields,
      // AP passwords, station passwords, the request body or credentials.
      auto before=receipt["previous_network"].to<JsonObject>();
      for(const char *key:{"station_enabled","ap_enabled","dhcp","station_ssid","auto_switch"})before[key]=d[key];
      receipt["target_boot_id"]=boot_;receipt["target_address"]=targetIp;
      if(!store(receipt))return "receipt_storage_failed";
    }
    const String encodedSsid=escaped(ssid_),encodedPassword=escaped(password_);
    if(encodedSsid.length()!=ssid_.length()*3||encodedPassword.length()!=password_.length()*3)return "request_memory_unavailable";
    constexpr const char *prefix="station_enabled=true&ap_enabled=true&dhcp=true&ssid=";
    const size_t formBytes=strlen(prefix)+encodedSsid.length()+strlen("&password=")+encodedPassword.length();
    String form;if(!form.reserve(formBytes))return "request_memory_unavailable";
    form=prefix;form+=encodedSsid;form+="&password=";form+=encodedPassword;
    if(form.length()!=formBytes)return "request_memory_unavailable";
    if(!phase("configuring","",true))return "receipt_storage_failed";
    dispatched=true;raw=request(address,"/api/network",&form,code,fenced);d.clear();
    if(code!=202||!fenced||deserializeJson(d,raw)||d["accepted"]!=true)return "configuration_outcome_uncertain_do_not_replay";
    // Read-only confirmation only. The peer applies network changes after1s;
    // AP remains enabled. A lost ACK/readback never triggers another POST.
    raw=request(address,"/api/network",nullptr,code,fenced);d.clear();
    if(code!=200||!fenced||deserializeJson(d,raw)||d["station_enabled"]!=true||d["ap_enabled"]!=true||d["dhcp"]!=true||d["station_ssid"]!=ssid_)
      return "configuration_readback_uncertain_do_not_replay";
    verified=true;return "";
  }
  void run() {
    bool dispatched=false,verified=false;const char *failure=commission(dispatched,verified);
    phase("restoring",failure);
    const bool restoredRequest=hooks_.restore();
    const uint32_t restoreAt=millis();
    while(restoredRequest&&!hooks_.restored()&&uint32_t(millis()-restoreAt)<30000U)vTaskDelay(pdMS_TO_TICKS(100));
    const bool ready=restoredRequest&&hooks_.restored();
    std::lock_guard<std::mutex> guard(mutex_);JsonDocument d;
    if(!deserializeJson(d,receipt_)){
      d["phase"]=*failure?(dispatched?"uncertain":"failed"):"complete";d["busy"]=false;
      d["error"]=failure;d["mutation_dispatched"]=dispatched;d["settings_verified"]=verified;
      d["restore_requested"]=true;d["restore_ready"]=ready;d["elapsed_ms"]=uint32_t(millis()-started_);
      if(!ready)d["restore_error"]="saved_network_reconnect_not_observed";
      if(!store(d)){d["receipt_persisted"]=false;receipt_=String();serializeJson(d,receipt_);}
    }
    // Release credentials and their backing allocations after the bounded job.
    for(size_t i=0;i<apPassword_.length();++i)apPassword_[i]=0;
    for(size_t i=0;i<password_.length();++i)password_[i]=0;
    apPassword_=static_cast<const char*>(nullptr);password_=static_cast<const char*>(nullptr);
    busy_=false;
  }
};
#endif
