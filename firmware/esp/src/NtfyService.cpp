#include "NtfyService.h"
#include "CloudConfig.h"
#include "CloudServices.h"
#include <BuildInfo.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif

NtfyService ntfyService;
void NtfyService::configurationChanged() {
  notices_.clear();
#if defined(ARDUINO_ARCH_ESP32)
  ++generation_;if(queue_)xQueueReset(queue_);
#endif
}
void NtfyService::observe(const PakarUpdate::Lifecycle &l,uint32_t now) {
  if(supported() && cloudConfig.ntfyEnabled())notices_.observe(l,now);
}
int NtfyService::status() const {
#if defined(ARDUINO_ARCH_ESP32)
  return status_.load();
#else
  return 0;
#endif
}
uint32_t NtfyService::accepted() const {
#if defined(ARDUINO_ARCH_ESP32)
  return accepted_.load();
#else
  return 0;
#endif
}
uint32_t NtfyService::failed() const {
#if defined(ARDUINO_ARCH_ESP32)
  return failed_.load();
#else
  return 0;
#endif
}
void NtfyService::service(bool networkReady,bool timeReady,bool maintenance,const char *hostname,uint32_t now) {
#if defined(ARDUINO_ARCH_ESP32)
  if(!cloudConfig.ntfyEnabled() || !notices_.pending() || maintenance || !networkReady || !timeReady) return;
  if(!queue_) {
    // Lazy opt-in allocation. TLS/HTTP runs on a low-priority worker, never in
    // the LED loop or async flash callback. Do not compete with OTA for memory.
    if(ESP.getFreeHeap()<100000U)return;
    queue_=xQueueCreate(1,sizeof(Job));if(!queue_)return;
    if(xTaskCreate(worker,"pakar-ntfy",10240,this,1,nullptr)!=pdPASS) {
      vQueueDelete(queue_);queue_=nullptr;return;
    }
  }
  PakarUpdate::NtfyNotice::Notice n;if(!notices_.take(n,now))return;
  Job job{};job.generation=generation_.load();
  strlcpy(job.host,cloudConfig.ntfyHost(),sizeof(job.host));
  strlcpy(job.topic,cloudConfig.ntfyTopic(),sizeof(job.topic));
  strlcpy(job.token,cloudConfig.ntfyToken(),sizeof(job.token));
  const bool reboot=!strcmp(n.phase,"awaiting_reboot") || !strcmp(n.phase,"ready-to-reboot");
  snprintf(job.message,sizeof(job.message),"%s · PakarController %s\n%s #%lu: %s%s",
    hostname,BoardCore::BuildInfo::kVersion,PakarUpdate::sourceName(n.source),static_cast<unsigned long>(n.operation),
    reboot?"Image verified; awaiting reboot":n.phase,n.verified?" (running image/target verified)":"");
  if(n.percent>=0 && !reboot) {
    const size_t used=strlen(job.message);
    snprintf(job.message+used,sizeof(job.message)-used,"\nMeasured stage: %d%%",n.percent);
  }
  // Coalesce unsent progress, never automatically retry an ambiguous POST.
  xQueueOverwrite(queue_,&job);
#else
  (void)networkReady;(void)timeReady;(void)maintenance;(void)hostname;(void)now;
#endif
}
#if defined(ARDUINO_ARCH_ESP32)
void NtfyService::worker(void *owner) {
  auto &self=*static_cast<NtfyService*>(owner);Job job{};
  for(;;) {
    if(xQueueReceive(self.queue_,&job,portMAX_DELAY)!=pdTRUE)continue;
    if(job.generation!=self.generation_.load())continue;
    int code=-1000;
    {
      // Adapt RemoteFirmware's verified CA/clock, direct origin, bounded HTTPS,
      // versioned user-agent and numeric-only error reporting. No redirects.
      WiFiClientSecure client;client.setCACert(pakarTrustAnchorPem());
      client.setConnectionTimeout(1500);client.setHandshakeTimeout(4);client.setTimeout(3000);
      HTTPClient http;http.setConnectTimeout(1500);http.setTimeout(3000);
      http.setReuse(false);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
      if(http.begin(client,String("https://")+job.host+"/"+job.topic)) {
        http.setUserAgent(String("PakarController/")+BoardCore::BuildInfo::kVersion);
        http.addHeader("Content-Type","text/plain; charset=utf-8");
        http.addHeader("Title","PakarController firmware update");
        http.addHeader("Tags","computer");
        if(job.token[0])http.addHeader("Authorization",String("Bearer ")+job.token);
        if(job.generation==self.generation_.load())code=http.POST(String(job.message));
        http.end();
      }
    }
    memset(job.token,0,sizeof(job.token));
    self.status_=code;
    if(code>=200 && code<300)++self.accepted_;else ++self.failed_;
    // HTTP acceptance is not evidence that a phone displayed the notification.
  }
}
#endif
