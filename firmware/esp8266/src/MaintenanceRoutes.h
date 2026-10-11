#pragma once
// Included inside the application's namespace after peripheral/transport code.
// PU850 route adaptation owns no duplicate UART/peripheral implementation.
ClientTelemetry clientTelemetry;
bool bootIntegrity=false;
String displayLocale="en";
String avrVersionText() {
  return String(avrVersion[0])+'.'+String(avrVersion[1])+'.'+String(avrVersion[2]);
}
uint16_t telemetry16(unsigned at) {return avrTelemetry[at]|static_cast<uint16_t>(avrTelemetry[at+1])<<8;}
uint32_t telemetry32(unsigned at) {
  return telemetry16(at)|static_cast<uint32_t>(telemetry16(at+2))<<16;
}
String encodedJson(const JsonDocument &d) {
  String s;const size_t expected=measureJson(d);
  return !d.overflowed() && serializeJson(d,s)==expected && s.length()==expected?s:String();
}
String clockJson() {
  JsonDocument d;d[F("epoch")]=static_cast<uint32_t>(time(nullptr));
  d[F("valid")]=time(nullptr)>1700000000;d[F("source")]=clockSource;
  d[F("ntp_configured")]=ntpConfigured;d[F("ntp_observed")]=ntpObserved;
  d[F("last_set_uptime_ms")]=lastClockSetAt;d[F("timezone")]=F("UTC");d[F("rtc_present")]=false;
  return encodedJson(d);
}
String networkTelemetry() {
  JsonDocument d;
  const bool connected=networkConfig.stationConnected();
  d[F("mode")]=static_cast<int>(WiFi.getMode());
  d[F("station")][F("configured")]=networkConfig.stationEnabled();
  d[F("station")][F("configured_ssid")]=networkConfig.stationSsid();
  d[F("station")][F("connected")]=connected;d[F("station")][F("status")]=static_cast<int>(WiFi.status());
  d[F("station")][F("ssid")]=connected?WiFi.SSID():String();
  d[F("station")][F("mac")]=WiFi.macAddress();d[F("station")][F("ip")]=WiFi.localIP().toString();
  d[F("station")][F("netmask")]=WiFi.subnetMask().toString();d[F("station")][F("gateway")]=WiFi.gatewayIP().toString();
  d[F("station")][F("dns0")]=WiFi.dnsIP(0).toString();d[F("station")][F("dns1")]=WiFi.dnsIP(1).toString();
  if(connected) {d[F("station")][F("rssi_dbm")]=WiFi.RSSI();d[F("station")][F("channel")]=WiFi.channel();}
  else {d[F("station")][F("rssi_dbm")]=nullptr;d[F("station")][F("channel")]=nullptr;}
  d[F("station")][F("dhcp")]=wifi_station_dhcpc_status()==DHCP_STARTED;
  d[F("ap")][F("enabled")]=apStarted;d[F("ap")][F("configured")]=networkConfig.accessPointEnabled();
  d[F("ap")][F("ssid")]=WiFi.softAPSSID();d[F("ap")][F("mac")]=WiFi.softAPmacAddress();
  d[F("ap")][F("ip")]=WiFi.softAPIP().toString();d[F("ap")][F("clients")]=WiFi.softAPgetStationNum();
  struct ip_info ip;
  if(wifi_get_ip_info(SOFTAP_IF,&ip)) d[F("ap")][F("netmask")]=IPAddress(ip.netmask.addr).toString();
  d[F("passwords")]=F("redacted");return encodedJson(d);
}
String buildTelemetryJson() {
  JsonDocument d;
  const uint32_t now=millis();
  d[F("type")]=F("telemetry");d[F("product")]=F("PakarController");
  d[F("hostname")]=hostname;d[F("fqdn")]=hostname+".local";d[F("boot_id")]=pcBootId;
  d[F("display_name")]=deviceIdentity.displayName();
  auto application=d[F("application")].to<JsonObject>();
  application[F("id")]=APP_ID;application[F("name")]=APP_NAME;application[F("title")]=APP_TITLE;
  d[F("version")]=BoardCore::BuildInfo::kVersion;
  auto e=d[F("esp")].to<JsonObject>();
  e[F("model")]=F("nodemcuv2");e[F("chip_id")]=ESP.getChipId();e[F("flash_chip_id")]=ESP.getFlashChipId();
  e[F("core_version")]=ESP.getCoreVersion();e[F("sdk_version")]=ESP.getSdkVersion();e[F("full_version")]=ESP.getFullVersion();
  e[F("cpu_mhz")]=ESP.getCpuFreqMHz();e[F("flash_bytes")]=ESP.getFlashChipSize();
  e[F("flash_real_bytes")]=ESP.getFlashChipRealSize();e[F("flash_hz")]=ESP.getFlashChipSpeed();
  e[F("flash_mode")]=static_cast<int>(ESP.getFlashChipMode());
  e[F("sketch_bytes")]=ESP.getSketchSize();e[F("sketch_md5")]=ESP.getSketchMD5();
  e[F("boot_integrity_verified")]=bootIntegrity;e[F("ota_free_bytes")]=ESP.getFreeSketchSpace();
  e[F("free_heap_bytes")]=ESP.getFreeHeap();e[F("largest_heap_block_bytes")]=ESP.getMaxFreeBlockSize();
  e[F("heap_fragmentation_percent")]=ESP.getHeapFragmentation();e[F("uptime_ms")]=now;
  e[F("reset_reason")]=ESP.getResetReason();e[F("reset_info")]=ESP.getResetInfo();
  FSInfo fs;if(LittleFS.info(fs)) {e[F("filesystem")][F("total_bytes")]=fs.totalBytes;e[F("filesystem")][F("used_bytes")]=fs.usedBytes;}
  e[F("uart_errors")]=link.errors();e[F("usb_errors")]=hostLink.errors();
  e[F("hardware")][F("ws2811_pin")]=F("D2/GPIO4");e[F("hardware")][F("pixels")]=480;e[F("hardware")][F("cells")]=80;
  e[F("hardware")][F("avr_rx")]=F("D4/GPIO2");e[F("hardware")][F("avr_tx")]=F("D0/GPIO16");
  e[F("hardware")][F("uart_baud")]=38400;e[F("hardware")][F("busy_pin")]=F("D3/GPIO0");
  e[F("hardware")][F("busy_wire")]=static_cast<bool>(PAKAR_USE_BUSY_WIRE);
  e[F("hardware")][F("isp_reset")]=F("D1/GPIO5");e[F("hardware")][F("isp_spi")]=F("D5,D6,D7");
  e[F("hardware")][F("i2c_present")]=false;
  auto a=d[F("avr")].to<JsonObject>();a[F("online")]=avrOnline;a[F("version_known")]=avrVersionKnown;
  if(avrVersionKnown) a[F("version")]=avrVersionText();else a[F("version")]=nullptr;
  a[F("model")]=F("atmega328p");a[F("flash_capacity_bytes")]=32768;a[F("sram_capacity_bytes")]=2048;
  a[F("eeprom_capacity_bytes")]=1024;a[F("features")]=avrPcFeatures;
  a[F("programming_latched")]=programmingLatched;a[F("last_frame_age_ms")]=static_cast<uint32_t>(now-lastAvrFrameAt);
  a[F("telemetry_supported")]=avrTelemetryKnown;
  const bool recent=avrOnline && avrTelemetryKnown && static_cast<uint32_t>(now-avrTelemetryAt)<=12000U;
  a[F("telemetry_recent")]=recent;
  if(avrTelemetryKnown) a[F("telemetry_age_ms")]=static_cast<uint32_t>(now-avrTelemetryAt);
  else a[F("telemetry_age_ms")]=nullptr;
  if(recent) {
    a[F("uptime_ms")]=telemetry32(4);a[F("free_sram_bytes")]=telemetry16(8);a[F("adc6_raw")]=telemetry16(10);
    a[F("buzzer")][F("frequency_hz")]=telemetry16(12);a[F("buzzer")][F("duration_ms")]=telemetry16(14);
    a[F("buzzer")][F("muted")]=static_cast<bool>(avrTelemetry[16]);a[F("uart_errors")]=telemetry16(42);
    a[F("lost_events")]=telemetry16(44);
    auto s=a[F("settings")].to<JsonObject>();s[F("storage_layout")]=avrTelemetry[17];s[F("flags")]=avrTelemetry[18];
    s[F("display_brightness")]=avrTelemetry[19];s[F("stream_period_ms")]=telemetry16(20);
    s[F("ninth_threshold")]=telemetry16(22);s[F("default_page")]=avrTelemetry[24];
    s[F("visible_pages")]=avrTelemetry[25];s[F("persisted")]=static_cast<bool>(avrTelemetry[38]);
    auto order=s[F("packed_order")].to<JsonArray>();for(unsigned i=26;i<29;++i) order.add(avrTelemetry[i]);
    const unsigned len=min<unsigned>(avrTelemetry[29],8);char name[9]={};memcpy(name,avrTelemetry+30,len);
    s[F("name")]=name;
  } else {
    a[F("uptime_ms")]=nullptr;a[F("free_sram_bytes")]=nullptr;a[F("adc6_raw")]=nullptr;
    a[F("buzzer")]=nullptr;a[F("settings")]=nullptr;a[F("lost_events")]=nullptr;a[F("uart_errors")]=nullptr;
  }
  a[F("hardware")][F("tm1637")]=F("A4/DIO,A5/CLK");a[F("hardware")][F("buzzer")]=F("D9/Timer1");
  a[F("hardware")][F("keys")]=9;a[F("hardware")][F("shift_outputs")]=8;a[F("hardware")][F("i2c_present")]=false;
  d[F("discovery")][F("mdns")]=mdnsStarted;d[F("discovery")][F("ssdp")]=ssdpStarted;d[F("discovery")][F("netbios")]=netbiosStarted;
  d[F("cloud")][F("mqtt_connected")]=cloudServices.mqttConnected();d[F("cloud")][F("mqtt_state")]=cloudServices.mqttState();
  d[F("cloud")][F("websocket_connected")]=cloudServices.webSocketConnected();
  d[F("cloud")][F("mqtt_messages_received")]=cloudServices.mqttMessagesReceived();
  d[F("cloud")][F("websocket_messages_received")]=cloudServices.webSocketMessagesReceived();
  d[F("cloud")][F("mqtt_password_set")]=cloudConfig.mqttPasswordSet();d[F("cloud")][F("websocket_token_set")]=cloudConfig.webSocketTokenSet();
  d[F("cloud")][F("telemetry_seconds")]=cloudConfig.telemetrySeconds();d[F("cloud")][F("error")]=cloudServices.lastError();
  d[F("auth")][F("ready")]=accessConfig.ready();d[F("auth")][F("default_credentials")]=accessConfig.defaultsActive();
  d[F("locale")]=displayLocale;d[F("locale_translation_available")]=false;
  d[F("client_capacity")]=8;d[F("client_ttl_ms")]=120000;
  // No credentials, arbitrary filesystem paths, synthetic voltage/temperature.
  if(d.overflowed()) return "{\"type\":\"telemetry-error\",\"error\":\"insufficient_heap\"}";
  String out=encodedJson(d);d.clear();
  if(out.length()<2)return "{\"type\":\"telemetry-error\",\"error\":\"insufficient_heap\"}";
  out.remove(out.length()-1);
  // Append serialized sections, releasing each tree before creating the next.
  // No deserialize-and-copy of 80 cells and eight client records into a second
  // aggregate tree. Failed String growth is detected, never partial JSON.
  auto add=[&](const char *key,const String &value) {
    if(value.isEmpty() || out.length()+strlen(key)+value.length()+5>16384)return false;
    const size_t expected=out.length()+strlen(key)+value.length()+4;
    out+=F(",\"");out+=key;out+=F("\":");out+=value;
    return out.length()==expected;
  };
  if(!add("network",networkTelemetry()) || !add("clock",clockJson()) ||
     !add("remote_firmware",remoteFirmware.json()) || !add("board",buildStatusJson()) ||
     !add("clients",clientTelemetry.json(now)))
    return "{\"type\":\"telemetry-error\",\"error\":\"insufficient_heap_or_snapshot_limit\"}";
  const size_t expected=out.length()+1;out+='}';
  return out.length()==expected?out:String("{\"type\":\"telemetry-error\",\"error\":\"insufficient_heap\"}");
}
bool prepareRemotePair(const RemoteFirmware::Plan &plan,String &reason) {
  if(artifactReaders || otaActive || otaRebootAt || avrUploadOwner || networkApplyPending) {reason=F("Another maintenance/network operation is active");return false;}
  if(!waitForAvr() || !avrVersionKnown || !avrPcSupported) {reason=F("AVR UART/profile unavailable; no erase allowed");return false;}
  if(plan.targets==1 && plan.esp.version!=avrVersionText()) {
    reason=F("ESP-only update would produce mismatched board versions; use target all");return false;
  }
  if(plan.targets==2 && plan.avr.version!=BoardCore::BuildInfo::kVersion) {
    reason=F("AVR-only update would produce mismatched board versions; use target all");return false;
  }
  if((plan.targets&2) && !plan.allowDowngrade &&
      RemotePolicy::compare(plan.avr.version.c_str(),avrVersionText().c_str())<0) {
    reason=F("AVR downgrade refused before erase");return false;
  }
  const bool retainRecovery=programmingLatched;
  if(!programmingLatched && !playUpdateStartCue()) {reason=F("AVR update cue not acknowledged");return false;}
  const uint8_t armed[]={1};
  if(!requestAndWaitForAck(kProgrammingState,armed,1)) {reason=F("AVR durable Prog arm not acknowledged; erase refused");return false;}
  programmingLatched=true;
  if(retainRecovery) {
    // An explicit new pinned repair must not overwrite the original known-good
    // flash with a half-updated application's contents. ISP still compares the
    // retained EEPROM to the actual target before any erase.
    File flash=LittleFS.open(String(kBackupDirectory)+"/flash.bin","r");
    File eeprom=LittleFS.open(String(kBackupDirectory)+"/eeprom.bin","r");
    File metadata=LittleFS.open(String(kBackupDirectory)+"/metadata.txt","r");
    const bool complete=flash && flash.size()==32768 && eeprom && eeprom.size()==1024 &&
        metadata && metadata.size()>0 && metadata.size()<=128;
    if(flash)flash.close();if(eeprom)eeprom.close();if(metadata)metadata.close();
    if(!complete){reason=F("Retained recovery snapshot missing; physical recovery required");return false;}
  } else {
    if(!avrIsp.backup(LittleFS,kBackupDirectory)) {reason=String("Recovery backup failed: ")+avrIsp.lastError();return false;}
    if(!waitForAvr()) {reason=F("AVR did not return from backup; Prog remains armed");return false;}
  }
  return true;
}
bool programRemoteAvr(const char *path,const String &version,String &reason) {
  File image=LittleFS.open(path,"r"),eeprom=LittleFS.open(String(kBackupDirectory)+"/eeprom.bin","r");
  const bool ok=avrIsp.programFlash(image,eeprom);image.close();eeprom.close();
  if(!ok) {reason=avrIsp.lastError();return false;}
  if(!waitForAvr() || !avrVersionKnown || avrVersionText()!=version) {
    reason=F("AVR flash verified but UART version did not match; Prog remains armed");return false;
  }
  const uint8_t armed[]={1};
  if(!requestAndWaitForAck(kProgrammingState,armed,1)) {reason=F("New AVR Prog acknowledgement missing");return false;}
  programmingLatched=true;return true;
}
int8_t finishRemotePair(const RemoteFirmware::Plan &plan,bool commit,String &reason) {
  if(!avrOnline || !avrVersionKnown || !avrPcSupported) {reason=F("Waiting for actual AVR version/profile");return 0;}
  if(avrVersionText()!=BoardCore::BuildInfo::kVersion ||
     ((plan.targets&2) && avrVersionText()!=plan.avr.version) ||
     ((plan.targets&1) && plan.esp.version!=BoardCore::BuildInfo::kVersion)) {
    reason=F("Actual board versions differ; Prog remains armed");return -1;
  }
  if(!commit) return 1;
  const uint8_t released[]={0};
  if(!requestAndWaitForAck(kProgrammingState,released,1)) {reason=F("Prog release outcome unknown; not retried");return -1;}
  programmingLatched=false;playUpdateSuccessCue();return 1;
}
bool strictU32(const String &s,uint32_t &out) {
  if(!s.length() || s.length()>10) return false;
  uint64_t v=0;for(size_t i=0;i<s.length();++i) {
    if(s[i]<'0'||s[i]>'9') return false;v=v*10+s[i]-'0';if(v>UINT32_MAX) return false;
  } out=v;return true;
}
bool remoteCommand(const JsonDocument &d,String &result) {
  const String command=d[F("command")]|"";
  bool ok=false;
  if(command=="firmware-status") {result=remoteFirmware.json();return true;}
  if(command=="telemetry") {result=buildTelemetryJson();return true;}
  if(command=="firmware-fetch") {
    if(artifactReaders || otaActive || otaRebootAt || avrUploadOwner || networkApplyPending) {
      result="Another operation is active or recovery is required";return false;
    }
    if(!d[F("url")].is<const char*>() || !d[F("sha256")].is<const char*>() ||
       !d[F("targets")].is<unsigned>() || (!d[F("apply")].isNull()&&!d[F("apply")].is<bool>()) ||
       (!d[F("allow_downgrade")].isNull()&&!d[F("allow_downgrade")].is<bool>()) ||
       (!d[F("bearer")].isNull()&&!d[F("bearer")].is<const char*>())) {result="Invalid typed firmware request";return false;}
    const unsigned targets=d[F("targets")];if(targets<1 || targets>3) {result="Invalid target";return false;}
    if(programmingLatched && (targets!=3 || (d[F("apply")]|false))) {
      result=F("Recovery requires a new pinned both-board stage and separate explicit apply");return false;
    }
    ok=remoteFirmware.queue(d[F("url")].as<String>(),d[F("sha256")].as<String>(),targets,
        d[F("apply")]|false,d[F("allow_downgrade")]|false,d[F("bearer")]|"");
  } else if(command=="firmware-apply" || command=="firmware-cancel") {
    if(artifactReaders) {result=F("Artifact download owns storage; retry after completion");return false;}
    if(!d[F("job_id")].is<uint32_t>() || !d[F("boot_id")].is<const char*>() ||
       (command=="firmware-apply"&&!d[F("sha256")].is<const char*>())) {result="Job identity required";return false;}
    ok=command=="firmware-apply"?remoteFirmware.apply(d[F("job_id")],d[F("boot_id")].as<String>(),d[F("sha256")].as<String>()):
        remoteFirmware.cancel(d[F("job_id")],d[F("boot_id")].as<String>());
  } else {result="Unknown firmware operation";return false;}
  result=ok?remoteFirmware.json():remoteFirmware.error();return ok;
}
void sendMaintenanceJson(const String &body,int code=200) {
  if(auto *request=server.request()) {
    auto *response=request->beginResponse(code,"application/json",body);
    response->addHeader("Cache-Control","no-store");request->send(response);
  }
}
bool handleMaintenanceShell(const String &line,String &result) {
  if(line=="info" || line=="telemetry") {result=buildTelemetryJson();return true;}
  String parts[6],rest=line;unsigned n=0;
  while(rest.length() && n<6) {
    const int space=rest.indexOf(' ');parts[n++]=space<0?rest:rest.substring(0,space);
    if(space<0){rest=String();break;}rest=rest.substring(space+1);rest.trim();
  }
  JsonDocument d;
  if(n==2 && parts[1]=="status") d[F("command")]=F("firmware-status");
  else if((n==4 || n==5) && parts[1]=="fetch") {
    d[F("command")]=F("firmware-fetch");d[F("url")]=parts[2];d[F("sha256")]=parts[3];
    d[F("targets")]=n==4 || parts[4]=="all"?3:parts[4]=="esp"?1:parts[4]=="avr"?2:0;
  } else if((n==5 && parts[1]=="apply") || (n==4 && parts[1]=="cancel")) {
    uint32_t id;if(!strictU32(parts[2],id)){result=F("ERR invalid job ID");return false;}
    d[F("command")]=parts[1]=="apply"?"firmware-apply":"firmware-cancel";
    d[F("job_id")]=id;d[F("boot_id")]=parts[3];d[F("sha256")]=parts[4];
  } else {result=F("ERR firmware status | fetch URL SHA [all|esp|avr] | apply JOB BOOT SHA | cancel JOB BOOT");return false;}
  return remoteCommand(d,result);
}
void registerMaintenanceRoutes() {
  bootIntegrity=ESP.checkFlashCRC();
  remoteFirmware.begin(pcBootId,prepareRemotePair,programRemoteAvr,finishRemotePair);
  File locale=LittleFS.open("/locale.cfg","r");
  if(locale && locale.size()==2) {char p[3]={};locale.read(reinterpret_cast<uint8_t*>(p),2);
    if(!strcmp(p,"en")||!strcmp(p,"fa")||!strcmp(p,"ar")||!strcmp(p,"ru")) displayLocale=p;
  }
  if(locale)locale.close();
  server.on("/api/telemetry",HTTP_GET,[](){if(authenticate())sendMaintenanceJson(buildTelemetryJson());});
  server.on("/api/clients/report",HTTP_POST,[](){
    if(!authenticate())return;
    const bool ok=clientTelemetry.report(server.arg("report"),server.request()->client()->remoteIP().toString(),millis());
    sendMaintenanceJson(ok?"{\"accepted\":true}":"{\"error\":\"invalid_report_or_client_limit\"}",ok?200:400);
  });
  server.on("/api/firmware/remote",HTTP_GET,[](){if(authenticate())sendMaintenanceJson(remoteFirmware.json());});
  const char *paths[]={"/api/firmware/fetch","/api/firmware/apply","/api/firmware/cancel","/file/cancel"};
  for(unsigned i=0;i<4;++i) {
    const unsigned op=i;
    server.on(paths[i],HTTP_POST,[op](){
      if(!authenticate())return;
      JsonDocument d;d[F("command")]=op==0?"firmware-fetch":op==1?"firmware-apply":"firmware-cancel";
      if(op==0) {
        uint32_t target=3;const String requested=server.arg("targets");
        if(requested.length() && !strictU32(requested,target)) {server.send(400,"text/plain","Invalid targets");return;}
        d[F("targets")]=target;d[F("url")]=server.arg("url");d[F("sha256")]=server.arg("sha256");
        if((server.hasArg("apply") && server.arg("apply")!="true" && server.arg("apply")!="false") ||
           (server.hasArg("allow_downgrade") && server.arg("allow_downgrade")!="true" && server.arg("allow_downgrade")!="false")) {
          server.send(400,"text/plain","Boolean values must be true or false");return;
        }
        d[F("apply")]=server.arg("apply")=="true";d[F("allow_downgrade")]=server.arg("allow_downgrade")=="true";
        d[F("bearer")]=server.arg("bearer");
      } else {
        uint32_t id;if(!strictU32(server.arg("job_id"),id)) {server.send(400,"text/plain","Job ID required");return;}
        d[F("job_id")]=id;d[F("boot_id")]=server.arg("boot_id");d[F("sha256")]=server.arg("sha256");
      }
      String result;const bool ok=remoteCommand(d,result);
      if(ok)sendMaintenanceJson(result,op==0?202:200);else server.send(409,"text/plain",result);
    });
  }
  for(const char *path:{"/datetime/get","/datetime/active","/ntp/get"})
    server.on(path,HTTP_GET,[](){if(authenticate())sendMaintenanceJson(clockJson());});
  for(const char *path:{"/datetime/parse","/datetime/set"}) {
    const bool apply=!strcmp(path,"/datetime/set");
    server.on(path,HTTP_POST,[apply](){
      if(!authenticate())return;
      uint32_t epoch;if(!RemotePolicy::utc(server.arg("val").c_str(),epoch)) {
        server.send(400,"text/plain","Use valid UTC YYYY-MM-DDTHH:MM:SSZ");return;
      }
      if(apply) {timeval tv={static_cast<time_t>(epoch),0};
        if(settimeofday(&tv,nullptr)) {server.send(500,"text/plain","Clock update failed");return;}}
      JsonDocument d;d[F("epoch")]=epoch;d[F("applied")]=apply;sendMaintenanceJson(encodedJson(d));
    });
  }
  for(const char *path:{"/wifi/status","/config/ap","/config/st"})
    server.on(path,HTTP_GET,[](){if(authenticate())sendMaintenanceJson(networkTelemetry());});
  server.on("/wifi/signal",HTTP_GET,[](){
    if(!authenticate())return;JsonDocument d;d[F("connected")]=networkConfig.stationConnected();
    if(networkConfig.stationConnected())d[F("rssi_dbm")]=WiFi.RSSI();else d[F("rssi_dbm")]=nullptr;
    sendMaintenanceJson(encodedJson(d));
  });
  server.on("/wifi/country",HTTP_GET,[](){
    if(!authenticate())return;wifi_country_t country{};JsonDocument d;
    if(wifi_get_country(&country)) {char cc[3]={country.cc[0],country.cc[1],0};d[F("country")]=cc;d[F("start_channel")]=country.schan;d[F("channels")]=country.nchan;}
    else d[F("country")]=nullptr;
    sendMaintenanceJson(encodedJson(d));
  });
  server.on("/hostname/check",HTTP_GET|HTTP_POST,[](){
    if(!authenticate())return;const String name=server.arg("value");
    if(!RemotePolicy::host(name.c_str(),name.length())) {server.send(400,"text/plain","Invalid DNS hostname");return;}
    if(remoteFirmware.busy()) {server.send(409,"text/plain","Firmware operation active");return;}
    IPAddress ip;const bool ok=WiFi.hostByName(name.c_str(),ip,3000)==1;
    JsonDocument d;d[F("hostname")]=name;d[F("resolved")]=ok;if(ok)d[F("ip")]=ip.toString();else d[F("ip")]=nullptr;
    sendMaintenanceJson(encodedJson(d),ok?200:404);
  });
  for(const char *path:{"/auth_dump","/sessions_dump","/auth","/username"})
    server.on(path,HTTP_GET,[](){
      if(!authenticate())return;JsonDocument d;d[F("username")]=accessConfig.username();
      d[F("ready")]=accessConfig.ready();d[F("secrets")]=F("redacted");d[F("sessions")]=webAccess.sessionCount();
      d[F("role")]=F("device-owner");d[F("settings")]=F("/api/auth");sendMaintenanceJson(encodedJson(d));
    });
  server.on("/pass",HTTP_POST,[](){
    if(!authenticate())return;
    if(programmingLatched || remoteFirmware.busy()) {server.send(409,"text/plain","Maintenance active");return;}
    if(!accessConfig.change(server.arg("current_password"),server.arg("username"),server.arg("password"))) {
      server.send(400,"text/plain",accessConfig.error());return;
    }
    webAccess.revokeAll();webSocket.closeAll(1008,"Credentials changed");
    if(telnetClient)telnetClient.stop();telnetAuthenticated=false;
    sendMaintenanceJson("{\"updated\":true,\"reauthenticate\":true}");
  });
  server.on("/random",HTTP_GET,[](){
    if(!authenticate())return;uint8_t bytes[32];char hex[65];
    if(os_get_random(bytes,sizeof(bytes))) {server.send(503,"text/plain","Entropy unavailable");return;}
    for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",bytes[i]);
    JsonDocument d;d[F("random_hex")]=hex;sendMaintenanceJson(encodedJson(d));
  });
  server.on("/lang",HTTP_GET,[](){if(authenticate()){JsonDocument d;d[F("locale")]=displayLocale;d[F("translated")]=false;sendMaintenanceJson(encodedJson(d));}});
  server.on("/lang",HTTP_POST,[](){
    if(!authenticate())return;String lang=server.arg("lang");
    if(lang=="E")lang="en";else if(lang=="F")lang="fa";else if(lang=="A")lang="ar";else if(lang=="R")lang="ru";
    if(lang!="en" && lang!="fa" && lang!="ar" && lang!="ru") {server.send(400,"text/plain","Unsupported locale");return;}
    File f=LittleFS.open("/locale.pending","w");
    const bool written=f && f.write(reinterpret_cast<const uint8_t*>(lang.c_str()),2)==2;
    if(f) {f.flush();f.close();}
    if(!written || !LittleFS.rename("/locale.pending","/locale.cfg")) {server.send(500,"text/plain","Locale storage failed");return;}
    displayLocale=lang;sendMaintenanceJson("{\"updated\":true,\"translated\":false}");
  });
  server.on("/beep",HTTP_POST,[](){
    if(!authenticate())return;uint32_t count=1;
    if((server.hasArg("count")&&!strictU32(server.arg("count"),count)) || count<1 || count>8 || programmingLatched) {
      server.send(400,"text/plain","Count must be 1..8 and Prog must be off");return;}
    const uint8_t tone[]={0x10,0x03,100,0};bool ok=true;
    for(unsigned i=0;i<count && ok;++i){ok=requestAndWaitForAck(kBuzzer,tone,4);if(ok)serviceAvrFor(160);}
    sendMaintenanceJson(ok?"{\"acknowledged\":true}":"{\"error\":\"avr_ack_missing\"}",ok?200:503);
  });
  server.on("/message/send",HTTP_POST,[](){
    if(!authenticate())return;const String text=server.arg("text");
    if(!text.length() || text.length()>40 || programmingLatched) {server.send(400,"text/plain","Text must be 1..40 characters; Prog must be off");return;}
    uint8_t p[44]={0,4,1,static_cast<uint8_t>(text.length())}; // 260 ms, single pass
    for(size_t i=0;i<text.length();++i) {if(text[i]<32 || text[i]>126){server.send(400,"text/plain","ASCII display text required");return;}p[4+i]=text[i];}
    // Legacy Pakar kDisplayText accepts flags/scroll/brightness/length.
    const bool ok=requestAndWaitForAck(kDisplayText,p,text.length()+4);
    sendMaintenanceJson(ok?"{\"acknowledged\":true}":"{\"error\":\"avr_ack_missing\"}",ok?200:503);
  });
  server.on("/crash",HTTP_GET|HTTP_POST,[](){if(authenticate())server.send(410,"text/plain","Unsafe memory corruption endpoint removed; diagnostics remain available");});
  for(const char *path:{"/debug/info","/debug/data","/eProcessState","/eResponseStatus"})
    server.on(path,HTTP_GET,[](){
      if(!authenticate())return;JsonDocument d;
      d[F("remote_phase")]=remoteFirmware.phase();d[F("remote_error")]=remoteFirmware.error();d[F("manual_ota_phase")]=otaPhase;
      d[F("avr_online")]=avrOnline;d[F("prog")]=programmingLatched;d[F("uart_errors")]=link.errors();d[F("usb_errors")]=hostLink.errors();
      d[F("last_reply_seen")]=awaitedReplySeen;
      if(awaitedReplySeen) {d[F("request_opcode")]=awaitedOpcode;d[F("acknowledged")]=awaitedReplyOk;} else d[F("request_opcode")]=nullptr;
      d[F("reset_reason")]=ESP.getResetReason();d[F("raw_uart_logging")]=false;
      sendMaintenanceJson(encodedJson(d));
    });
  server.on("/debug/clear",HTTP_POST,[](){
    if(!authenticate())return;awaitedReplySeen=false;awaitedReplyOk=false;
    sendMaintenanceJson("{\"cleared\":true,\"persistent_update_journal_preserved\":true,\"error_counters_reset\":false}");
  });
}
