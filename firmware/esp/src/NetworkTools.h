#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <ping/ping_sock.h>
#include <mutex>
#include <atomic>
#include "PeerLanResolver.h"
#include "NetworkToolsSettings.h"

// Exactly one diagnostic job; bounded DNS, HTTP payload and ping count. Only
// the loop owner persists settings or sets the clock. Worker callbacks never
// touch the renderer, filesystem, client sockets or application JSON trees.
class NetworkTools {
 public:
  bool busy()const{return busy_.load();}
  bool submit(const String &kind,const String &target,String &error){
    std::lock_guard<std::mutex> lock(mutex_);
    if(busy()||completed_){error="network_tools_busy";return false;}
    if(kind!="dns"&&kind!="ping"&&kind!="http-date"&&kind!="public-ip"){error="unsupported_network_probe";return false;}
    if(target.length()!=strlen(target.c_str())||((kind=="dns"||kind=="ping")?
        !PakarNetworkObservation::hostname(target.c_str()):!NetworkToolsSettings::url(target))){
      error="invalid_probe_target";return false;
    }
    const bool tls=kind=="http-date"||kind=="public-ip";
    const size_t stack=worker_?0:8192;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<(tls?50000:18000)+stack||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<(tls?18000:stack?10000:4096)){
      error="network_tools_low_memory";return false;
    }
    if(!worker_&&xTaskCreate(worker,"pakar-net-tools",8192,this,1,&worker_)!=pdPASS){error="worker_unavailable";return false;}
    kind_=kind;target_=target;failure_="";value_="";httpStatus_=0;dateEpoch_=0;received_=sent_=rtt_=0;
    started_=millis();++revision_;busy_=true;queued_=true;return true;
  }
  void startPending(){std::lock_guard<std::mutex> lock(mutex_);if(queued_){queued_=false;xTaskNotifyGive(worker_);}}
  String snapshot(){
    std::lock_guard<std::mutex> lock(mutex_);JsonDocument d;
    d["revision"]=revision_;d["busy"]=busy();d["kind"]=kind_;d["target"]=target_;
    d["phase"]=busy()?"running":revision_?(failure_.length()?"failed":"complete"):"idle";
    d["error"]=failure_;d["value"]=value_;d["http_status"]=httpStatus_;
    d["elapsed_ms"]=busy()?uint32_t(millis()-started_):elapsed_;
    d["observed_uptime_ms"]=observed_;d["observation_age_ms"]=observed_?uint32_t(millis()-observed_):0;
    d["date_epoch"]=dateEpoch_;d["tls_verified"]=dateEpoch_>0||(kind_=="public-ip"&&value_.length()>0);
    d["ping_sent"]=sent_;d["ping_received"]=received_;d["ping_last_rtt_ms"]=rtt_;
    d["dns_record_type"]="A";d["traceroute_supported"]=false;
    String out;serializeJson(d,out);return out;
  }
  // Drain one completed observation before admitting another job. This also
  // prevents a fast client overwriting a result before the loop publishes it.
  bool takeCompletion(uint32_t &dateEpoch){
    std::lock_guard<std::mutex> lock(mutex_);if(!completed_)return false;
    completed_=false;dateEpoch=dateEpoch_;return true;
  }
  void appendObserved(JsonObject d){
    std::lock_guard<std::mutex> lock(mutex_);
    d["public_ip"]=publicIp_;d["public_ip_observed"]=publicAt_>0;
    d["public_ip_observed_uptime_ms"]=publicAt_;
    d["public_ip_age_ms"]=publicAt_?uint32_t(millis()-publicAt_):0;
    d["public_ip_source"]=publicSource_;d["http_date_epoch"]=lastDate_;
    d["http_date_age_ms"]=dateAt_?uint32_t(millis()-dateAt_):0;
    d["http_date_source"]=dateSource_;d["http_date_tls_verified"]=lastDate_>0;
    d["public_ip_family"]="IPv4";
  }
 private:
  std::mutex mutex_;std::atomic<bool> busy_{false};TaskHandle_t worker_=nullptr;
  PeerLanResolver resolver_;bool completed_=false,queued_=false;
  String kind_,target_,failure_,value_,publicIp_,publicSource_,dateSource_;
  uint32_t revision_=0,started_=0,elapsed_=0,observed_=0,dateEpoch_=0,publicAt_=0,dateAt_=0,lastDate_=0;
  int httpStatus_=0;uint32_t sent_=0,received_=0,rtt_=0;
  static void worker(void *p){auto &s=*static_cast<NetworkTools*>(p);for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);s.run();}}
  class PinnedTls final:public NetworkClientSecure {
   public:
    explicit PinnedTls(IPAddress ip):ip_(ip),deadline_(millis()+9000U){useBuiltinCACertBundle();setTimeout(2500);setHandshakeTimeout(4);}
    int connect(const char *host,uint16_t port)override{return connect(host,port,2500);}
    int connect(const char *host,uint16_t port,int32_t timeout)override{
      setTimeout(timeout);return NetworkClientSecure::connect(ip_,port,host,nullptr,nullptr,nullptr);
    }
    int available()override{return expired()?0:NetworkClientSecure::available();}
    uint8_t connected()override{return expired()?0:NetworkClientSecure::connected();}
    int read()override{return expired()?-1:NetworkClientSecure::read();}
    int read(uint8_t *data,size_t count)override{return expired()?-1:NetworkClientSecure::read(data,count);}
   private:
    IPAddress ip_;uint32_t deadline_;
    bool expired(){if(int32_t(millis()-deadline_)<0)return false;stop();return true;}
  };
  void run(){
    String failure,value;uint32_t date=0,sent=0,received=0,rtt=0;int status=0;
    const bool http=kind_=="http-date"||kind_=="public-ip";
    const int slash=http?target_.indexOf('/',8):-1;
    const String host=http?(slash<0?target_.substring(8):target_.substring(8,slash)):target_;
    IPAddress ip;
    if(!ip.fromString(host)&&!resolver_.lookup(host,ip))failure="dns_query_failed_or_deadline";
    else if(ip[0]==0||ip[0]>=224||ip==IPAddress(255,255,255,255))failure="invalid_unicast_target";
    else if(kind_=="dns")value=ip.toString();
    else if(kind_=="ping"){
      esp_ping_config_t config=ESP_PING_DEFAULT_CONFIG();config.count=3;config.interval_ms=200;config.timeout_ms=700;config.data_size=16;
      IP_SET_TYPE_VAL(config.target_addr,IPADDR_TYPE_V4);ip4_addr_set_u32(ip_2_ip4(&config.target_addr),uint32_t(ip));
      // The SDK owns callback/task lifetime. The worker remains until on_end,
      // so neither callback arguments nor the session handle can dangle.
      struct PingResult {std::atomic<bool> done{false};uint32_t sent=0,received=0,rtt=0;} result;
      esp_ping_callbacks_t cb{};cb.cb_args=&result;
      cb.on_ping_success=[](esp_ping_handle_t h,void *p){auto &r=*static_cast<PingResult*>(p);esp_ping_get_profile(h,ESP_PING_PROF_TIMEGAP,&r.rtt,sizeof(r.rtt));};
      cb.on_ping_end=[](esp_ping_handle_t h,void *p){auto &r=*static_cast<PingResult*>(p);
        esp_ping_get_profile(h,ESP_PING_PROF_REQUEST,&r.sent,sizeof(r.sent));
        esp_ping_get_profile(h,ESP_PING_PROF_REPLY,&r.received,sizeof(r.received));r.done=true;};
      esp_ping_handle_t handle=nullptr;
      if(esp_ping_new_session(&config,&cb,&handle)!=ESP_OK)failure="ping_session_unavailable";
      else {
        if(esp_ping_start(handle)!=ESP_OK)failure="ping_start_failed";
        else {while(!result.done.load())vTaskDelay(1);sent=result.sent;received=result.received;rtt=result.rtt;value=ip.toString();if(!received)failure="icmp_no_reply";}
        esp_ping_delete_session(handle);
      }
    } else if(!PakarNetworkObservation::publicIpv4(ip.toString().c_str()))failure="lookup_service_requires_public_ipv4";
    else if(time(nullptr)<1700000000)failure="clock_required_for_verified_https_use_ntp";
    else {
      PinnedTls client(ip);HTTPClient request;request.setConnectTimeout(2500);request.setTimeout(2500);
      request.setReuse(false);request.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);request.setUserAgent(String("PakarController/")+BoardCore::BuildInfo::kVersion);
      const char *headers[]={"Date","Age"};request.collectHeaders(headers,2);
      if(!request.begin(client,target_))failure="https_begin_failed";
      else {
        status=kind_=="http-date"?request.sendRequest("HEAD"):request.GET();
        if(status<200||status>=300)failure="https_lookup_failed";
        else if(kind_=="http-date"){
          const String raw=request.header("Date"),age=request.header("Age");
          if(raw.length()!=29||!PakarNetworkObservation::httpDate(raw.c_str(),date))failure="invalid_http_date";
          else if(age.length()&&age!="0"){date=0;failure="cached_http_date_rejected";}
          else {value=raw;const int64_t skew=int64_t(date)-int64_t(time(nullptr));
            if(skew>300||skew<-300){date=0;failure="http_date_disagrees_with_clock";}}
        }else {
          // Never getString() on an unbounded Internet response.
          const int length=request.getSize();
          if(length<1||length>64)failure="public_ip_response_size_invalid";
          else {char bytes[65]{};const size_t got=request.getStreamPtr()->readBytes(bytes,length);
            if(got!=size_t(length)||memchr(bytes,0,length))failure="public_ip_response_incomplete";
            else {value=String(bytes);value.trim();if(!PakarNetworkObservation::publicIpv4(value.c_str())){value="";failure="invalid_public_ipv4";}}}
        }
        request.end();
      }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    failure_=failure;value_=value;dateEpoch_=date;httpStatus_=status;sent_=sent;received_=received;rtt_=rtt;
    observed_=millis();elapsed_=uint32_t(observed_-started_);
    if(!failure.length()&&kind_=="public-ip"){publicIp_=value;publicAt_=observed_;publicSource_=target_;}
    if(date){lastDate_=date;dateAt_=observed_;dateSource_=target_;}
    completed_=true;busy_=false;
  }
};
#endif
