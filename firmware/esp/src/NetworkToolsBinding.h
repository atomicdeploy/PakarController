#pragma once
// Application loop-owned binding. The existing command router supplies remote
// identity, expiry, deduplication and authentication, including MQTT/WS commands.
String networkToolsJson(){
  JsonDocument d;d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;
  networkToolsSettings.append(d["settings"].to<JsonObject>());
  const time_t epoch=time(nullptr);
  auto clock=d["clock"].to<JsonObject>();clock["utc_epoch"]=epoch>1700000000?uint64_t(epoch):0;
  clock["valid"]=epoch>1700000000;clock["source"]=clockSource;clock["ntp_observed"]=ntpObserved;
  clock["ntp_authenticated"]=false;clock["last_set_uptime_ms"]=lastClockSetAt;clock["uptime_ms"]=millis();
#if defined(ARDUINO_ARCH_ESP32)
  d["supported"]=bool(PAKAR_NETWORK_ENABLED);d["dhcp_ntp_supported"]=true;
  d["dhcp_ntp_policy_applies_on_next_lease"]=true;
  auto servers=clock["effective_ntp_servers"].to<JsonArray>();
  for(unsigned i=0;i<3;++i){const ip_addr_t *address=esp_sntp_getserver(i);
    if(address&&!ip_addr_isany(address)){char text[48];ipaddr_ntoa_r(address,text,sizeof(text));servers.add(text);}
    else {const char *name=esp_sntp_getservername(i);if(name&&*name)servers.add(name);}}
  networkTools.appendObserved(d["observed"].to<JsonObject>());
  JsonDocument job;if(!deserializeJson(job,networkTools.snapshot()))d["job"]=job.as<JsonObjectConst>();
#else
  d["supported"]=false;d["dhcp_ntp_supported"]=false;
#endif
  d["traceroute_supported"]=false;
  return ConfigurationManagement::json(d);
}
bool setBoardClock(JsonVariantConst epoch,String &result){
  if(ConfigurationManagement::busy()||urbootBusy.get()||!epoch.is<uint32_t>()||
      epoch.as<uint32_t>()<1704067200UL||epoch.as<uint32_t>()>4102444799UL){result="invalid_utc_epoch_or_maintenance_busy";return false;}
  timeval value{time_t(epoch.as<uint32_t>()),0};
  if(settimeofday(&value,nullptr)){result="clock_update_failed";return false;}
#if defined(ARDUINO_ARCH_ESP32)
  ntpSyncPending=false;
#endif
  clockSource="client";lastClockSetAt=millis();
  PakarConfiguration::changes().applied(PakarConfiguration::NetworkTools);cloudServices.requestTelemetry();
  result=networkToolsJson();return true;
}
bool updateNetworkTools(JsonObjectConst patch,String &result){
  if(!ConfigurationManagement::start(patch,result)||artifactReaders){
    if(!result.length())result="network_tools_or_maintenance_busy";return false;
  }
  NetworkToolsSettings next=networkToolsSettings;
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::ScopedLedFlashPause pause;if(!pause){result="led_flash_guard_busy";return false;}
#endif
  if(!next.patch(patch,result)||!next.save(result))return false;
  const bool clockChanged=next.ntpEnabled!=networkToolsSettings.ntpEnabled||next.ntpDhcp!=networkToolsSettings.ntpDhcp||next.ntpServer!=networkToolsSettings.ntpServer;
  networkToolsSettings=next;
  if(clockChanged){ntpConfigured=false;configureNtp();}
#if defined(ARDUINO_ARCH_ESP32)
  nextDateLookup=nextPublicIpLookup=0;
#endif
  PakarConfiguration::changes().saved(PakarConfiguration::NetworkTools);
  PakarConfiguration::changes().applied(PakarConfiguration::NetworkTools);
  cloudServices.requestTelemetry();result=networkToolsJson();return true;
}
String wifiScanJson(bool request){
  JsonDocument d;int count=WiFi.scanComplete();
  if(request&&count!=-1){
    if(!PAKAR_NETWORK_ENABLED)d["error"]="station_scan_unavailable";
    else if(ConfigurationManagement::busy()||artifactReaders||cloudServices.ioActive())d["error"]="maintenance_busy";
    else {wifiScanQueued=true;wifiScanReportPending=true;count=-1;}
  }
  if(wifiScanQueued)count=-1;
  if(!request&&count<-1&&!wifiScanQueued)d["error"]="scan_unavailable_or_failed";
  d["running"]=count==-1;d["complete"]=count>=0;d["accepted"]=d["error"].isNull();
  // The SDK enables the station radio for scanning while retaining the AP;
  // requiring a saved station SSID here would prevent first-time provisioning.
  d["scan_supported"]=bool(PAKAR_NETWORK_ENABLED);
  d["total_found"]=count>=0?count:0;d["truncated"]=count>12;d["max_results"]=12;
  auto rows=d["networks"].to<JsonArray>();
  for(int i=0;i<count&&i<12;++i){auto row=rows.add<JsonObject>();row["ssid"]=WiFi.SSID(i);row["rssi"]=WiFi.RSSI(i);
    row["security"]=WiFi.encryptionType(i);row["channel"]=WiFi.channel(i);
#if defined(ARDUINO_ARCH_ESP32)
    row["encrypted"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
#else
    row["encrypted"]=WiFi.encryptionType(i)!=ENC_TYPE_NONE;
#endif
  }
  return ConfigurationManagement::json(d);
}
bool submitNetworkProbe(const String &kind,const String &requested,String &result){
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_NETWORK_ENABLED
  if(ConfigurationManagement::busy()||artifactReaders||!NetworkPath::ready()||urbootBusy.get()||uploadFile){result="network_or_maintenance_busy";return false;}
  const String target=kind=="http-date"?networkToolsSettings.dateUrl:kind=="public-ip"?networkToolsSettings.publicIpUrl:requested;
  if(!networkTools.submit(kind,target,result))return false;
  ++artifactReaders;networkToolsLease=true;result=networkTools.snapshot();return true;
#else
  (void)kind;(void)requested;result="network_tools_not_built";return false;
#endif
}
void serviceNetworkTools(){
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_NETWORK_ENABLED
  if(wifiScanReportPending&&!wifiScanQueued&&WiFi.scanComplete()!=-1){
    wifiScanReportPending=false;
    JsonDocument event,scan;event["type"]="wifi-scan";event["board_id"]=pcBoardId;event["boot_id"]=pcBootId;
    if(!deserializeJson(scan,wifiScanJson(false)))event["scan"]=scan.as<JsonObjectConst>();
    String body;serializeJson(event,body);publishPcEvent(body);cloudServices.requestTelemetry();
  }
  if(ntpSyncPending.exchange(false)&&networkToolsSettings.ntpEnabled){
    clockSource="ntp";ntpObserved=true;lastClockSetAt=millis();
    PakarConfiguration::changes().applied(PakarConfiguration::NetworkTools);cloudServices.requestTelemetry();
  }
  if(!cloudServices.ioActive())networkTools.startPending();
  uint32_t date=0;
  if(networkToolsLease&&networkTools.takeCompletion(date)){
    --artifactReaders;networkToolsLease=false;
    // NTP stays authoritative when running. HTTP can maintain a previously
    // initialized clock if NTP has been explicitly disabled, never TLS bypass.
    if(date&&!networkToolsSettings.ntpEnabled){timeval value{time_t(date),0};
      if(!settimeofday(&value,nullptr)){clockSource="https-date";lastClockSetAt=millis();}}
    JsonDocument event;event["type"]="network-observation";event["board_id"]=pcBoardId;event["boot_id"]=pcBootId;
    JsonDocument job;if(!deserializeJson(job,networkTools.snapshot()))event["job"]=job.as<JsonObjectConst>();
    String body;serializeJson(event,body);publishPcEvent(body);cloudServices.requestTelemetry();
  }
  const uint32_t now=millis();
  if(!NetworkPath::ready()||time(nullptr)<1700000000||networkToolsLease)return;
  const uint32_t period=networkToolsSettings.refreshSeconds*1000UL;
  String result;
  if(networkToolsSettings.dateAutomatic&&(!nextDateLookup||int32_t(now-nextDateLookup)>=0)){
    nextDateLookup=now+period;submitNetworkProbe("http-date","",result);
  }else if(networkToolsSettings.publicIpAutomatic&&(!nextPublicIpLookup||int32_t(now-nextPublicIpLookup)>=0)){
    nextPublicIpLookup=now+period;submitNetworkProbe("public-ip","",result);
  }
#endif
}
