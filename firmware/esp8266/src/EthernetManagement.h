#pragma once
// Included after CloudManagement inside the loop owner's namespace.
String ethernetStatusJson() {
  JsonDocument d;
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  const auto &port=NetworkPath::ethernet();
  d["configured"]=true;d["initialized"]=port.initialized();
  d["enabled"]=ethernetConfig.enabled();
  d["started"]=port.started();d["link_up"]=port.linkUp();
  d["ipv4_ready"]=port.connected();d["dhcp"]=ethernetConfig.dhcpEnabled();
  if(port.connected()) {
    d["ip"]=port.localIP().toString();d["gateway"]=port.gatewayIP().toString();
    d["subnet"]=port.subnetMask().toString();
    d["dns1"]=port.dnsIP(0).toString();d["dns2"]=port.dnsIP(1).toString();
  } else {
    for(const char *key:{"ip","gateway","subnet","dns1","dns2"})d[key]=nullptr;
  }
  const String mac=port.macAddress();
  if(mac.length())d["mac"]=mac;else d["mac"]=nullptr;
  if(port.linkUp()){d["speed_mbps"]=port.linkSpeedMbps();d["full_duplex"]=port.fullDuplex();}
  else {d["speed_mbps"]=nullptr;d["full_duplex"]=nullptr;}
  PakarS3::LldpStatus lldp;port.lldpStatus(lldp);
  auto info=d["lldp"].to<JsonObject>();
  info["enabled"]=ethernetConfig.lldpEnabled();info["receive_requested"]=ethernetConfig.lldpReceive();
  info["transmit_supported"]=lldp.transmitSupported;info["receive_supported"]=lldp.receiveSupported;
  info["receive_active"]=lldp.receiveActive;info["promiscuous"]=lldp.promiscuous;
  info["job_pending"]=lldp.jobPending;info["transmitted"]=lldp.transmitted;
  info["transmit_failed"]=lldp.transmitFailed;info["received"]=lldp.received;
  info["invalid"]=lldp.invalid;info["dropped"]=lldp.dropped;info["queue_rejected"]=lldp.queueRejected;
  info["last_error"]=lldp.lastError;
  auto neighbors=info["neighbors"].to<JsonArray>();
  for(unsigned i=0;i<lldp.neighborCount&&i<PakarS3::Lldp::maxNeighbors;++i) {
    const auto &source=lldp.neighbors[i];auto row=neighbors.add<JsonObject>();
    char address[18];snprintf(address,sizeof(address),"%02x:%02x:%02x:%02x:%02x:%02x",
        source.mac[0],source.mac[1],source.mac[2],source.mac[3],source.mac[4],source.mac[5]);
    row["mac"]=address;row["chassis_subtype"]=source.chassisSubtype;row["chassis"]=source.chassis;
    row["port_subtype"]=source.portSubtype;row["port"]=source.port;
    row["system_name"]=source.systemName;row["description"]=source.description;
    row["ttl_seconds"]=source.ttl;row["age_ms"]=uint32_t(millis()-source.observedAt);
  }
  d["last_error"]=port.lastError();
#else
  d["configured"]=false;
#endif
  // Cheap live observations only: never sketch hashing, flash reads or probes.
  return ConfigurationManagement::json(d);
}
String ethernetConfigurationJson() {
  JsonDocument d;
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  d["configured"]=true; d["dhcp"]=ethernetConfig.dhcpEnabled();
  d["enabled"]=ethernetConfig.enabled();d["lldp_enabled"]=ethernetConfig.lldpEnabled();
  d["lldp_receive"]=ethernetConfig.lldpReceive();
  d["address"]=ethernetConfig.address().toString();
  d["gateway"]=ethernetConfig.gateway().toString();
  d["subnet"]=ethernetConfig.subnet().toString();
  d["dns1"]=ethernetConfig.dns1().toString(); d["dns2"]=ethernetConfig.dns2().toString();
  d["last_error"]=ethernetConfig.lastError();
  d["apply_error"]=NetworkPath::ethernet().lastError();
  d["ipv4_ready"]=NetworkPath::ethernet().connected();
  d["apply_pending"]=networkApplyPending && ethernetApplyOnly;
#else
  d["configured"]=false;
#endif
  return ConfigurationManagement::json(d);
}
bool updateEthernetConfiguration(JsonObjectConst patch,String &result) {
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  if(!ConfigurationManagement::start(patch,result))return false;
  EthernetConfigStore candidate=ethernetConfig;
  bool dhcp=candidate.dhcpEnabled();
  bool enabled=candidate.enabled(),lldp=candidate.lldpEnabled(),receive=candidate.lldpReceive();
  String address=candidate.address().toString(),gateway=candidate.gateway().toString();
  String subnet=candidate.subnet().toString(),dns1=candidate.dns1().toString(),dns2=candidate.dns2().toString();
  ConfigurationManagement::Patch read{patch,result};
  if(!read.boolean("dhcp",dhcp) || !read.boolean("enabled",enabled) ||
      !read.boolean("lldp_enabled",lldp) || !read.boolean("lldp_receive",receive) || !read.text("address",address,15) ||
      !read.text("gateway",gateway,15) || !read.text("subnet",subnet,15) ||
      !read.text("dns1",dns1,15) || !read.text("dns2",dns2,15))return false;
  if(!read.supplied) {result=F("No Ethernet configuration fields supplied");return false;}
  const auto station=WiFi.localIP();
  const bool stationReady=networkConfig.stationConnected()&&(station[0]||station[1]||station[2]||station[3]);
  const bool accessPointReady=networkConfig.accessPointEnabled()&&apStarted;
  if(!enabled&&!stationReady&&!accessPointReady) {
    result=F("Connect Wi-Fi or enable its access point before disabling Ethernet");return false;
  }
  const bool changed=address!=candidate.address().toString() || gateway!=candidate.gateway().toString() ||
      subnet!=candidate.subnet().toString() || dns1!=candidate.dns1().toString() || dns2!=candidate.dns2().toString();
  if(!candidate.setAddressing(dhcp&&!changed,address,gateway,subnet,dns1,dns2) ||
      (dhcp&&changed&&!candidate.setAddressing(true))) {result=candidate.lastError();return false;}
  candidate.setServices(enabled,lldp,receive);
  if(!ConfigurationManagement::acknowledgement(result))return false;
  if(!candidate.save()) {result=candidate.lastError();return false;}
  ethernetConfig=candidate;
  ethernetApplyOnly=true;networkApplyAt=millis()+1000U;networkApplyPending=true;
  return true;
#else
  (void)patch;result=F("Ethernet is not compiled into this firmware");return false;
#endif
}
