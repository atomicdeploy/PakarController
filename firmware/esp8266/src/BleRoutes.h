#pragma once
// Included only for ESP32, after the shared configuration/command/maintenance
// services. This adapter has no HTTP request object or Wi-Fi loopback dependency.
// All device effects go through the existing loop-owner handlers/receipt cache.
namespace BleRoutes {
bool readOnly=false,servicing=false;
constexpr const char *routes[]={
  "GET /api/status","GET /api/table/snapshot","GET /api/cells/mapping",
  "GET /api/telemetry","GET /api/auth/info","GET /api/device/name","GET /api/status-rgb",
  "GET /api/network","GET /api/network/ethernet","GET /api/cloud",
  "GET /api/firmware/remote","GET /api/update/lifecycle","GET /api/display/lcd",
  "GET /api/avr/contracts","GET /api/avr/contracts/result","GET /api/ble","GET /api/ble/clock",
  "POST /api/command","POST /api/cell","POST /api/cells","POST /api/effect",
  "POST /api/mode","POST /api/brightness","POST /api/led/output",
  "POST /api/audio","POST /api/buzzer","POST /api/display","POST /api/outputs",
  "POST /api/game/reset","POST /api/avr/contracts","POST /api/device/name",
  "POST /api/network","POST /api/network/ethernet","POST /api/cloud",
  "POST /api/cloud/reconnect","POST /api/firmware/fetch","POST /api/firmware/apply",
  "POST /api/firmware/cancel","POST /api/ble","POST /api/status-rgb"
};
bool supported(const String &method,const String &path) {
  const String key=method+" "+path;for(const char *route:routes)if(key==route)return true;return false;
}
bool strings(JsonObjectConst form) {
  if(form.size()>32)return false;
  for(JsonPairConst field:form) {
    if(!field.value().is<const char*>() || strlen(field.value().as<const char*>())!=field.value().as<JsonString>().size())return false;
  }
  return true;
}
bool number(JsonObjectConst form,const char *key,JsonObject into,const char *target=nullptr,bool optional=false) {
  if(form[key].isUnbound())return optional;
  const String text=form[key].as<String>();if(text.isEmpty()||text.length()>11)return false;
  const bool negative=text[0]=='-';size_t at=negative?1U:0U;
  if(at==text.length())return false;
  uint32_t value=0;
  for(;at<text.length();++at){const char c=text[at];if(c<'0'||c>'9'||value>(UINT32_MAX-static_cast<uint32_t>(c-'0'))/10U)return false;value=value*10U+c-'0';}
  const char *out=target?target:key;
  if(negative){if(value>2147483648UL)return false;into[out]=-static_cast<int64_t>(value);}
  else into[out]=value;
  return true;
}
bool boolean(JsonObjectConst form,const char *key,JsonObject into,bool optional=true) {
  if(form[key].isUnbound())return optional;
  const String value=form[key].as<String>();
  if(value!="true"&&value!="false"&&value!="1"&&value!="0")return false;
  into[key]=value=="true"||value=="1";return true;
}
bool configuration(JsonObjectConst form,bool cloud,JsonDocument &document,bool ethernet=false) {
  auto patch=document.to<JsonObject>();
  const char *cloudText[]={"mqtt_host","mqtt_user","mqtt_password","mqtt_base_topic","websocket_host","websocket_path","websocket_token"};
  const char *networkText[]={"ssid","password","address","gateway","subnet","dns1","dns2"};
  const char *cloudBool[]={"mqtt_enabled","mqtt_tls","websocket_enabled","websocket_tls"};
  const char *networkBool[]={"station_enabled","ap_enabled","dhcp"};
  for(unsigned i=ethernet?2U:0U;i<7;++i){const char *key=cloud?cloudText[i]:networkText[i];if(!form[key].isUnbound())patch[key]=form[key];}
  for(unsigned i=ethernet?2U:0U;i<(cloud?4U:3U);++i)if(!boolean(form,cloud?cloudBool[i]:networkBool[i],patch))return false;
  if(cloud)for(const char *key:{"mqtt_port","websocket_port","telemetry_seconds"})if(!number(form,key,patch,nullptr,true))return false;
  // Reject unknown fields instead of accepting a typo and applying a partial
  // configuration. The shared owner still performs ranges/persistence/readback.
  return patch.size()==form.size() && !document.overflowed();
}
String response(const String &id,int status,const String &body) {
  JsonDocument d;d["type"]="ble-response";d["request_id"]=id;
  d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;d["status"]=status;
  String out=encodedJson(d);if(out.isEmpty())return String();out.remove(out.length()-1);
  out+=F(",\"body\":");out+=body;out+='}';
  return out.length()<=65535?out:String();
}
String error(const String &id,int status,const char *reason) {
  JsonDocument d;d["accepted"]=false;d["error"]=reason;return response(id,status,encodedJson(d));
}
String mapping() {
  JsonDocument d;d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;
  d["logical_order"]="row-major";d["odd_column_offset"]=0.5;
  for(uint8_t i=0;i<LedGrid::kCells;++i){d["physical"].add(cellMapping.physical[i]);
    if(cellMapping.names[i][0])d["names"].add(cellMapping.names[i]);else d["names"].add(String(i+1));}
  return encodedJson(d);
}
String read(const String &path,JsonObjectConst form,int &status) {
  status=200;
  if(path=="/api/auth/info") {
    JsonDocument d;d["ready"]=accessConfig.ready();d["enabled"]=accessConfig.enabled();
    d["ble_auth_supported"]=false;d["credentials_over_ble"]=false;return encodedJson(d);
  }
  if(path=="/api/status") {
    String body=buildStatusJson();
    if(form["pixels"]=="1") {body.remove(body.length()-1);body+=F(",\"pixel_preview_requested\":true,\"pixel_frame\":");body+=grid.pixelFrameJson();body+='}';}
    return body;
  }
  if(path=="/api/table/snapshot")return buildUsbTableSnapshot();
  if(path=="/api/cells/mapping")return mapping();
  if(path=="/api/telemetry")return buildTelemetryJson();
  if(path=="/api/device/name")return deviceNameJson();
  if(path=="/api/status-rgb")return statusRgbConfigurationJson();
  if(path=="/api/network")return networkConfigurationJson();
  if(path=="/api/network/ethernet")return ethernetConfigurationJson();
  if(path=="/api/cloud")return cloudConfigurationJson();
  if(path=="/api/firmware/remote")return remoteFirmware.json();
  if(path=="/api/update/lifecycle")return upgradeJson();
  if(path=="/api/display/lcd")return lcdStatusJson();
  if(path=="/api/ble")return bleStatusJson();
  if(path=="/api/ble/clock") {JsonDocument d;d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;d["command_clock_ms"]=commandClock();return encodedJson(d);}
  if(path=="/api/avr/contracts") {
    JsonDocument d;d["dialect"]="pakar-pccontroller";d["profile"]=80;
    d["tunnel_opcode"]=86;d["chunk_opcode"]=87;d["maximum_payload"]=48;
    d["supported"]=avrOnline&&avrPcSupported;
    if(avrOnline&&avrPcSupported)d["enabled_features"]=avrPcFeatures;else d["enabled_features"]=nullptr;
    d["esp_addressable_led"]=static_cast<bool>(PAKAR_PC_ESP_LED);d["boot_id"]=pcBootId;return encodedJson(d);
  }
  if(path=="/api/avr/contracts/result") {
    uint32_t id=0;if(!strictU32(form["id"].as<String>(),id)||!id){status=400;return "{\"error\":\"invalid_receipt_id\"}";}
    const auto *receipt=pcTransport.get(id);
    if(!receipt||receipt->usb){status=404;return "{\"error\":\"unknown_contract_request\"}";}
    return pcReceiptJson(*receipt);
  }
  status=404;return "{\"error\":\"unsupported_ble_route\"}";
}
bool write(const String &path,JsonObjectConst form,String &body,int &status) {
  status=202;JsonDocument d;auto command=d.to<JsonObject>();String result;
  if(path=="/api/status-rgb")return updateStatusRgbConfiguration(form,body,status);
  if(path=="/api/ble") {
    if(form.size()!=1||!boolean(form,"enabled",command,false)){status=400;return false;}
    if(!bleService.configure(command["enabled"].as<bool>(),result)){body=result;status=503;return false;}
    body=bleStatusJson();return true;
  }
  if(path=="/api/network"||path=="/api/cloud"||path=="/api/network/ethernet") {
    if(!configuration(form,path=="/api/cloud",d,path=="/api/network/ethernet")){status=400;return false;}
    const bool ok=path=="/api/cloud"?updateCloudConfiguration(d.as<JsonObjectConst>(),result,false):
      path=="/api/network"?updateNetworkConfiguration(d.as<JsonObjectConst>(),result):updateEthernetConfiguration(d.as<JsonObjectConst>(),result);
    body=result;return ok;
  }
  if(path=="/api/cloud/reconnect") {cloudApplyPending=true;cloudApplyAt=millis()+1000U;body="{\"accepted\":true,\"apply_after_ms\":1000}";return true;}
  if(path=="/api/device/name") {
    const String label=form["display_name"].isUnbound()?String(deviceIdentity.displayName()):form["display_name"].as<String>();
    const bool ok=scheduleDeviceIdentity(form["hostname"].as<String>(),label);body=deviceNameJson(ok);return ok;
  }
  if(path=="/api/firmware/fetch"||path=="/api/firmware/apply"||path=="/api/firmware/cancel") {
    command["command"]=path=="/api/firmware/fetch"?"firmware-fetch":path=="/api/firmware/apply"?"firmware-apply":"firmware-cancel";
    if(path=="/api/firmware/fetch") {
      if(!number(form,"targets",command,nullptr,true)||!boolean(form,"apply",command)||!boolean(form,"allow_downgrade",command)){status=400;return false;}
      if(form["targets"].isUnbound())command["targets"]=3;
      for(const char *key:{"url","sha256","bearer"})command[key]=form[key].isUnbound()?String():form[key].as<String>();
    } else {
      if(!number(form,"job_id",command)){status=400;return false;}
      command["boot_id"]=form["boot_id"];command["sha256"]=form["sha256"];
    }
    const bool ok=remoteCommand(d,result);body=result;return ok;
  }
  const char *name=path=="/api/cell"?"cell":path=="/api/cells"?"cells":path=="/api/effect"?"effect":
    path=="/api/mode"?"mode":path=="/api/brightness"?"brightness":path=="/api/led/output"?"led-output":
    path=="/api/audio"?"audio":path=="/api/buzzer"?"buzzer":path=="/api/display"?"display":
    path=="/api/outputs"?"outputs":path=="/api/game/reset"?"reset":path=="/api/avr/contracts"?"pc-contract":nullptr;
  if(!name){status=404;return false;}
  for(JsonPairConst field:form)command[field.key()]=field.value();command["command"]=name;
  if(path=="/api/cell"&&!number(form,"id",command)){status=400;return false;}
  if(path=="/api/brightness"&&!number(form,"value",command)){status=400;return false;}
  if(path=="/api/buzzer"&&(!number(form,"frequency",command)||!number(form,"duration_ms",command))){status=400;return false;}
  if(path=="/api/outputs"&&!number(form,"mask",command)){status=400;return false;}
  if(path=="/api/avr/contracts"&&!number(form,"opcode",command)){status=400;return false;}
  if(path=="/api/led/output"&&!boolean(form,"enabled",command)){status=400;return false;}
  if(path=="/api/display") {
    if(!boolean(form,"release",command)||!boolean(form,"force_scroll",command)){status=400;return false;}
    for(const char *key:{"value","speed_ms","hold_ms","interval_seconds"})if(!number(form,key,command,nullptr,true)){status=400;return false;}
  }
  const String packet=encodedJson(d);if(packet.isEmpty()){status=503;return false;}
  const bool accepted=handleCloudCommand(packet,result);body=result;return accepted;
}
} // namespace BleRoutes

String bleStatusJson() {
  JsonDocument d;d["configured"]=static_cast<bool>(PAKAR_S3_BLE_ENABLED);d["enabled"]=bleService.enabled();
  d["requested_enabled"]=bleService.requestedEnabled();
  d["started"]=bleService.started();d["connected"]=bleService.connected();d["apply_pending"]=bleService.applyPending();
  d["reboot_required"]=bleService.rebootRequired();d["build_required"]=bleService.buildRequired();
  d["startup_attempted"]=bleService.startupAttempted();
  d["service_uuid"]=PakarS3::BleService::serviceUuid;d["rx_uuid"]=PakarS3::BleService::rxUuid;
  d["tx_uuid"]=PakarS3::BleService::txUuid;d["info_uuid"]=PakarS3::BleService::infoUuid;
  d["auth_required"]=!accessConfig.ready()||accessConfig.enabled();d["credentials_over_ble"]=false;
  d["request_limit"]=8192;d["response_limit"]=65535;d["max_packet"]=512;
  d["mtu"]=bleService.mtu();d["partial_timeout_ms"]=5000;d["assembly_limit_ms"]=60000;d["intent_lifetime_ms"]=8000;
  d["firmware_binary"]=false;d["raw_stream"]=false;d["wifi_scan"]=false;
  d["rejected"]=bleService.rejected();d["epoch"]=bleService.epoch();
  d["events_lost"]=bleService.eventsLost();d["events_pending"]=bleService.eventsPending();d["event_max_age_ms"]=1500;
  d["event_loss_policy"]="disconnect-and-resynchronize";
  d["advertisement_name"]=hostname.substring(0,29);d["error"]=bleService.error();
  for(const char *route:BleRoutes::routes)d["routes"].add(route);
  for(const char *event:{"status","pc-contract-event","pc-contract-result","update-progress"})d["push"].add(event);
  return encodedJson(d);
}
void publishBleEvent(const String &body) {
  JsonDocument parsed;if(body.length()>1536||deserializeJson(parsed,body)||!parsed["type"].is<const char*>())return;
  JsonDocument head;head["type"]="ble-event";head["event"]=parsed["type"];
  head["board_id"]=pcBoardId;head["boot_id"]=pcBootId;head["received_uptime_ms"]=millis();
  String event=encodedJson(head);if(event.isEmpty())return;event.remove(event.length()-1);
  event+=F(",\"body\":");event+=body;event+='}';bleService.publish(event);
}
String bleRequest(const String &message,uint32_t epoch) {
  JsonDocument d;if(message.length()>8192||deserializeJson(d,message)||!d.is<JsonObject>())return BleRoutes::error("",400,"invalid_ble_request");
  const auto request=d.as<JsonObjectConst>();const String id=PakarCommand::stringValue(request,"request_id");
  bool validId=id.length()==32;for(size_t i=0;i<id.length();++i)validId &= (id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f');
  const String method=PakarCommand::stringValue(request,"method"),path=PakarCommand::stringValue(request,"path");
  if(!validId||request["type"]!="ble-request"||(method!="GET"&&method!="POST")||!request["form"].is<JsonObject>()||
     !BleRoutes::strings(request["form"].as<JsonObjectConst>()))return BleRoutes::error(id,400,"invalid_ble_request");
  if(!BleRoutes::supported(method,path))return BleRoutes::error(id,404,"unsupported_ble_route");
  if(!accessConfig.ready()||accessConfig.enabled())return BleRoutes::error(id,401,"ble_authentication_not_supported");
  if(!bleService.owns(epoch))return BleRoutes::error(id,409,"ble_session_changed");
  const auto form=request["form"].as<JsonObjectConst>();
  if(method=="GET") {int status=200;const String body=BleRoutes::read(path,form,status);
    return body.length()?BleRoutes::response(id,status,body):BleRoutes::error(id,503,"snapshot_memory_unavailable");}
  if(BleRoutes::readOnly)return BleRoutes::error(id,409,"maintenance_read_only");
  const uint64_t now=commandClock();
  if(PakarCommand::stringValue(request,"board_id")!=pcBoardId||PakarCommand::stringValue(request,"boot_id")!=pcBootId||!webAccess.peerFenceReady())return BleRoutes::error(id,409,"peer_changed");
  if(!request["expires_at_ms"].is<uint64_t>()||request["expires_at_ms"].as<uint64_t>()<=now||request["expires_at_ms"].as<uint64_t>()-now>8000)return BleRoutes::error(id,408,"command_expired");
  // /api/command already owns a hash-bound receipt on its original inner ID.
  // Avoid reserving the same user intent twice with transport-specific bytes.
  if(path=="/api/command") {
    if(ConfigurationManagement::busy()||programmingLatched||urbootBusy.get())return BleRoutes::error(id,409,"device_busy");
    const String body=commandReply(form["packet"].as<String>());return BleRoutes::response(id,200,body);
  }
  FirmwarePlatform::Sha256 hash;hash.add(message.c_str(),message.length());
  const String lease=String(epoch)+":"+pcBootId;hash.add(lease.c_str(),lease.length());
  BoardCore::CommandReceipts::Entry *entry=nullptr;
  const auto admission=commandReceipts.reserve(id.c_str(),hash.hex().c_str(),request["expires_at_ms"],now,entry);
  if(admission==BoardCore::CommandReceipts::Cached) {
    JsonDocument cached;cached["accepted"]=entry->accepted;cached["cached"]=true;cached["readback_required"]=true;
    // This reports consumed admission, not a fabricated completion/job ID.
    return BleRoutes::response(id,200,encodedJson(cached));
  }
  if(admission!=BoardCore::CommandReceipts::Fresh)return BleRoutes::error(id,409,
    admission==BoardCore::CommandReceipts::Full?"command_capacity":admission==BoardCore::CommandReceipts::Conflict?"request_id_conflict":"invalid_or_expired_request");
  if(!bleService.owns(epoch)||!accessConfig.ready()||accessConfig.enabled()||commandClock()>=request["expires_at_ms"].as<uint64_t>())return BleRoutes::error(id,409,"ble_session_changed");
  if((ConfigurationManagement::busy()||programmingLatched||urbootBusy.get()||passiveCaptureActive())&&path!="/api/firmware/cancel")return BleRoutes::error(id,409,"device_busy");
  String result;int status=202;entry->accepted=BleRoutes::write(path,form,result,status);
  if(!entry->accepted){JsonDocument failed;failed["accepted"]=false;failed["error"]=result.length()?result:String("invalid_or_rejected_request");return BleRoutes::response(id,status==202?409:status,encodedJson(failed));}
  JsonDocument parsed;
  if(result.length()&&!deserializeJson(parsed,result)&&parsed.is<JsonObject>())return BleRoutes::response(id,status,result);
  JsonDocument ack;ack["accepted"]=true;ack["state"]="queued";ack["message"]=result;
  if(path=="/api/cells") {BoardCore::CellBatch selection;CellState state;Effect effect;
    if(parseCellsAction(form["ids"].as<String>(),form["state"].as<String>(),form["effect"].as<String>(),selection,state,effect))ack["count"]=selection.count();}
  return BleRoutes::response(id,status,encodedJson(ack));
}
void serviceBle() {
  if(BleRoutes::servicing)return;BleRoutes::servicing=true;
  const bool allowed=accessConfig.ready()&&!accessConfig.enabled();bleService.authorization(allowed);
  static uint32_t infoAt=0;
  if(!infoAt||static_cast<uint32_t>(millis()-infoAt)>=100U) {
    infoAt=millis();JsonDocument info;info["product"]="PakarController";info["board_id"]=pcBoardId;info["boot_id"]=pcBootId;
    info["hostname"]=hostname;info["name"]=deviceIdentity.displayName();info["auth_required"]=!allowed;
    info["ble"]["enabled"]=bleService.enabled();info["ble"]["commands"]=allowed;
    info["ble"]["firmware_binary"]=false;info["ble"]["max_packet"]=512;
    bleService.setInfo(encodedJson(info));
  }
  bleService.service(millis(),urbootBusy.get()||otaActive||otaRebootAt||remoteFirmware.busy(),hostname);
  if(bleService.canPush(millis())) {
    String event=F("{\"type\":\"ble-event\",\"event\":\"status\",\"board_id\":\"");event+=pcBoardId;
    event+=F("\",\"boot_id\":\"");event+=pcBootId;event+=F("\",\"body\":");event+=buildStatusJson();event+='}';
    bleService.push(event,millis());
  }
  BleRoutes::servicing=false;
}
void serviceBleReadOnly() {
  if(BleRoutes::servicing)return;
  BleRoutes::readOnly=true;serviceBle();BleRoutes::readOnly=false;
}
void registerBleApi() {
  server.on("/api/ble",HTTP_GET,[](){if(authenticate())server.send(200,"application/json",bleStatusJson());});
  server.on("/api/ble",HTTP_POST,[](){
    if(!authenticate())return;
    if(ConfigurationManagement::busy()){server.send(409,"application/json","{\"error\":\"device_busy\"}");return;}
    const String value=server.arg("enabled");if(value!="true"&&value!="false"&&value!="0"&&value!="1"){server.send(400,"application/json","{\"error\":\"invalid_enabled\"}");return;}
    String error;if(!bleService.configure(value=="true"||value=="1",error)){server.send(503,"text/plain",error);return;}
    server.send(202,"application/json",bleStatusJson());
  });
}
