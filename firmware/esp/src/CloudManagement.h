#pragma once
// Included inside the loop owner's namespace. Transport authentication/peer
// fencing belongs to the caller; candidate validation/commit is shared here.
namespace ConfigurationManagement {
inline bool busy() {
  return remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner ||
      programmingLatched || networkApplyPending || cloudApplyPending;
}
inline String json(const JsonDocument &document) {
  String result;
  const size_t expected=measureJson(document);
  if(document.overflowed() || expected>4096 ||
      serializeJson(document,result)!=expected || result.length()!=expected)
    return F("{\"error\":\"configuration_snapshot_unavailable\"}");
  return result;
}
struct Patch {
  JsonObjectConst object;
  String &error;
  bool supplied=false;
  bool boolean(const char *key,bool &out) {
    const JsonVariantConst value=object[key];
    if(value.isUnbound())return true;
    supplied=true;
    if(!value.is<bool>()) {error=String(key)+F(" must be a boolean");return false;}
    out=value.as<bool>();return true;
  }
  bool number(const char *key,uint16_t &out,uint16_t maximum=65535) {
    const JsonVariantConst value=object[key];
    if(value.isUnbound())return true;
    supplied=true;
    if(!value.is<uint32_t>() || value.as<uint32_t>()<1 || value.as<uint32_t>()>maximum) {
      error=String(key)+F(" is outside its integer range");return false;
    }
    out=static_cast<uint16_t>(value.as<uint32_t>());return true;
  }
  bool text(const char *key,String &out,size_t maximum) {
    const JsonVariantConst value=object[key];
    if(value.isUnbound())return true;
    supplied=true;
    if(!value.is<const char*>()) {error=String(key)+F(" must be a string");return false;}
    const JsonString text=value.as<JsonString>();
    if(text.size()>maximum || memchr(text.c_str(),'\0',text.size())) {
      error=String(key)+F(" is too long or contains NUL");return false;
    }
    out=value.as<String>();
    if(out.length()!=text.size()) {error=F("Configuration allocation failed");return false;}
    return true;
  }
};
inline bool start(JsonObjectConst patch,String &result) {
  if(busy()) {result=F("Device maintenance/network/cloud change active");return false;}
  if(patch.isNull() || patch.size()>32) {result=F("Invalid configuration object");return false;}
  return true;
}
inline bool acknowledgement(String &result) {
  static constexpr char response[]="{\"accepted\":true,\"apply_pending\":true,\"apply_after_ms\":1000}";
  result=response;
  return result.length()==sizeof(response)-1;
}
} // namespace ConfigurationManagement

String cloudConfigurationJson() {
  JsonDocument d;
  d[F("mqtt_enabled")]=cloudConfig.mqttEnabled();d[F("mqtt_tls")]=cloudConfig.mqttTls();
  d[F("mqtt_host")]=cloudConfig.mqttHost();d[F("mqtt_port")]=cloudConfig.mqttPort();
  d[F("mqtt_user")]=cloudConfig.mqttUser();d[F("mqtt_password_set")]=cloudConfig.mqttPasswordSet();
  d[F("mqtt_base_topic")]=cloudConfig.mqttBaseTopic();
  d[F("mqtt_connected")]=cloudServices.mqttConnected();d[F("mqtt_state")]=cloudServices.mqttState();
  d[F("websocket_enabled")]=cloudConfig.webSocketEnabled();d[F("websocket_tls")]=cloudConfig.webSocketTls();
  d[F("websocket_host")]=cloudConfig.webSocketHost();d[F("websocket_port")]=cloudConfig.webSocketPort();
  d[F("websocket_path")]=cloudConfig.webSocketPath();d[F("websocket_token_set")]=cloudConfig.webSocketTokenSet();
  d[F("websocket_connected")]=cloudServices.webSocketConnected();
  d[F("telemetry_seconds")]=cloudConfig.telemetrySeconds();d[F("last_error")]=cloudServices.lastError();
  d[F("apply_pending")]=cloudApplyPending;
  if(NtfyService::supported()) {
    d["ntfy_enabled"]=cloudConfig.ntfyEnabled();d["ntfy_host"]=cloudConfig.ntfyHost();
    d["ntfy_topic"]=cloudConfig.ntfyTopic();d["ntfy_token_set"]=cloudConfig.ntfyTokenSet();
    d["ntfy_delivery"]["pending"]=ntfyService.pending();
    d["ntfy_delivery"]["http_status"]=ntfyService.status();
    d["ntfy_delivery"]["accepted"]=ntfyService.accepted();d["ntfy_delivery"]["failed"]=ntfyService.failed();
  }
  return ConfigurationManagement::json(d);
}

bool updateCloudConfiguration(JsonObjectConst patch,String &result,bool internetCommand=false) {
  if(!ConfigurationManagement::start(patch,result))return false;
  CloudConfigStore candidate=cloudConfig;
  bool mqtt=candidate.mqttEnabled(),mqttTls=candidate.mqttTls();
  bool ws=candidate.webSocketEnabled(),wsTls=candidate.webSocketTls();
  uint16_t mqttPort=candidate.mqttPort(),wsPort=candidate.webSocketPort(),seconds=candidate.telemetrySeconds();
  String host=candidate.mqttHost(),user=candidate.mqttUser(),password,topic=candidate.mqttBaseTopic();
  String wsHost=candidate.webSocketHost(),path=candidate.webSocketPath(),token;
  bool ntfy=candidate.ntfyEnabled();
  String ntfyHost=candidate.ntfyHost(),ntfyTopic=candidate.ntfyTopic(),ntfyToken;
  ConfigurationManagement::Patch read{patch,result};
  if(!read.boolean("mqtt_enabled",mqtt) || !read.boolean("mqtt_tls",mqttTls) ||
      !read.boolean("websocket_enabled",ws) || !read.boolean("websocket_tls",wsTls) ||
      !read.number("mqtt_port",mqttPort) || !read.number("websocket_port",wsPort) ||
      !read.number("telemetry_seconds",seconds,3600) ||
      !read.text("mqtt_host",host,63) || !read.text("mqtt_user",user,32) ||
      !read.text("mqtt_password",password,64) || !read.text("mqtt_base_topic",topic,64) ||
      !read.text("websocket_host",wsHost,63) || !read.text("websocket_path",path,96) ||
      !read.text("websocket_token",token,64) || !read.boolean("ntfy_enabled",ntfy) ||
      !read.text("ntfy_host",ntfyHost,63) || !read.text("ntfy_topic",ntfyTopic,64) ||
      !read.text("ntfy_token",ntfyToken,64))return false;
  if(ntfy && !NtfyService::supported()) {result=F("ntfy HTTPS worker is unavailable on this build");return false;}
  if(!read.supplied) {result=F("No cloud configuration fields supplied");return false;}
  if(internetCommand && ((mqtt&&!mqttTls)||(ws&&!wsTls))) {
    result=F("Internet configuration requires TLS for every enabled cloud service");return false;
  }
  if(!candidate.setMqtt(mqtt,mqttTls,host,mqttPort,user,password,topic,
          !patch["mqtt_password"].isUnbound()) ||
      !candidate.setWebSocket(ws,wsTls,wsHost,wsPort,path,token,
          !patch["websocket_token"].isUnbound()) || !candidate.setTelemetrySeconds(seconds) ||
      !candidate.setNtfy(ntfy,ntfyHost,ntfyTopic,ntfyToken,!patch["ntfy_token"].isUnbound())) {
    result=candidate.lastError();return false;
  }
  // Prepare a bounded response before the irreversible durable commit. Never
  // publish a partially changed global config or reconnect inside the handler.
  if(!ConfigurationManagement::acknowledgement(result))return false;
  if(!candidate.save()) {result=candidate.lastError();return false;}
  cloudConfig=candidate;
  ntfyService.configurationChanged();
  cloudApplyAt=millis()+1000U;cloudApplyPending=true;
  return true;
}

String networkConfigurationJson() {
  JsonDocument d;
  d[F("station_enabled")]=networkConfig.stationEnabled();
  d[F("station_connected")]=networkConfig.stationConnected();
  d[F("ap_enabled")]=networkConfig.accessPointEnabled();d[F("station_ssid")]=networkConfig.stationSsid();
  d[F("station_ip")]=WiFi.localIP().toString();d[F("ap_ip")]=WiFi.softAPIP().toString();
  d[F("dhcp")]=networkConfig.dhcpEnabled();d[F("address")]=networkConfig.stationAddress().toString();
  d[F("gateway")]=networkConfig.stationGateway().toString();d[F("subnet")]=networkConfig.stationSubnet().toString();
  d[F("dns1")]=networkConfig.stationDns1().toString();d[F("dns2")]=networkConfig.stationDns2().toString();
  d[F("uplink")][F("interface")]=NetworkPath::interfaceName();
  d[F("uplink")][F("ipv4_ready")]=NetworkPath::ready();
#if defined(ARDUINO_ARCH_ESP32)
  d[F("ethernet")][F("configured")]=NetworkPath::ethernet().configured();
  d[F("ethernet_ready")]=NetworkPath::ethernet().connected();
#else
  d[F("ethernet")][F("configured")]=false;
  d[F("ethernet_ready")]=false;
#endif
  d[F("apply_pending")]=networkApplyPending;
  d["auto_switch"]=networkConfig.autoSwitch();d["active_profile"]=networkConfig.activeProfile();
  auto profiles=d["station_profiles"].to<JsonArray>();
  for(uint8_t i=0;i<4;++i)if(networkConfig.profileSsid(i)[0]){auto p=profiles.add<JsonObject>();p["priority"]=i;p["ssid"]=networkConfig.profileSsid(i);}
  return ConfigurationManagement::json(d);
}

bool updateNetworkConfiguration(JsonObjectConst patch,String &result) {
  if(!ConfigurationManagement::start(patch,result))return false;
  NetworkConfigStore candidate=networkConfig;
  bool station=candidate.stationEnabled(),ap=candidate.accessPointEnabled(),dhcp=candidate.dhcpEnabled();
  String ssid=candidate.stationSsid(),password;
  String address=candidate.stationAddress().toString(),gateway=candidate.stationGateway().toString();
  String subnet=candidate.stationSubnet().toString(),dns1=candidate.stationDns1().toString(),dns2=candidate.stationDns2().toString();
  ConfigurationManagement::Patch read{patch,result};
  if(!read.boolean("station_enabled",station)||!read.boolean("ap_enabled",ap)||!read.boolean("dhcp",dhcp)||
      !read.text("ssid",ssid,32)||!read.text("password",password,63)||
      !read.text("address",address,15)||!read.text("gateway",gateway,15)||
      !read.text("subnet",subnet,15)||!read.text("dns1",dns1,15)||!read.text("dns2",dns2,15))return false;
  bool autoSwitch=candidate.autoSwitch();
  if(!read.boolean("auto_switch",autoSwitch))return false;
  candidate.setAutoSwitch(autoSwitch);
  if(!patch["fallbacks"].isUnbound()){
    if(!patch["fallbacks"].is<JsonArrayConst>()||patch["fallbacks"].size()>3){result=F("fallbacks must contain at most three ordered profiles");return false;}
    for(uint8_t i=0;i<3;++i)candidate.setFallback(i,String(),String());
    uint8_t i=0;for(JsonVariantConst value:patch["fallbacks"].as<JsonArrayConst>()){
      if(!value["ssid"].is<const char*>()||!value["password"].is<const char*>()||!candidate.setFallback(i++,value["ssid"].as<String>(),value["password"].as<String>())){result=F("invalid fallback profile; explicit password required");return false;}
    }
    read.supplied=true;
  }
  if(!read.supplied) {result=F("No network configuration fields supplied");return false;}
  if(!dhcp)for(uint8_t i=1;i<4;++i)if(candidate.profileSsid(i)[0]){result=F("Multiple station networks require DHCP");return false;}
  // Disabling the final Wi-Fi path is an explicit Ethernet-only handoff, never
  // justified by a compiled driver or a client-provided readiness boolean.
  if(!station&&!ap) {
#if defined(ARDUINO_ARCH_ESP32)
    if(!NetworkConfigStore::ethernetBuild()||!NetworkPath::ethernet().connected()) {
      result=F("Ethernet must have a live IPv4 path before Wi-Fi can be disabled");return false;
    }
#else
    result=F("AP and station cannot both be disabled without native Ethernet");return false;
#endif
  }
  const bool changedAddress=address!=candidate.stationAddress().toString() ||
      gateway!=candidate.stationGateway().toString() || subnet!=candidate.stationSubnet().toString() ||
      dns1!=candidate.stationDns1().toString() || dns2!=candidate.stationDns2().toString();
  if(!candidate.setStation(ssid,password,station,!patch["password"].isUnbound()) ||
      !candidate.setAccessPointEnabled(ap) ||
      // Validate/store edited static profile first, then restore DHCP without
      // ever applying a temporary static mode to the live interface.
      !candidate.setAddressing(dhcp&&!changedAddress,address,gateway,subnet,dns1,dns2) ||
      (dhcp&&changedAddress&&!candidate.setAddressing(true))) {
    result=candidate.lastError();return false;
  }
  if(!ConfigurationManagement::acknowledgement(result))return false;
  if(!candidate.save()) {result=candidate.lastError();return false;}
  networkConfig=candidate;
  networkApplyAt=millis()+1000U;networkApplyPending=true;
  return true;
}
