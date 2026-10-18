#pragma once
// Included after CloudManagement inside the loop owner's namespace.
String ethernetStatusJson() {
  JsonDocument d;
#if defined(ARDUINO_ARCH_ESP32) && PAKAR_S3_ETHERNET_ENABLED
  const auto &port=NetworkPath::ethernet();
  d["configured"]=true;d["initialized"]=port.initialized();
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
  String address=candidate.address().toString(),gateway=candidate.gateway().toString();
  String subnet=candidate.subnet().toString(),dns1=candidate.dns1().toString(),dns2=candidate.dns2().toString();
  ConfigurationManagement::Patch read{patch,result};
  if(!read.boolean("dhcp",dhcp) || !read.text("address",address,15) ||
      !read.text("gateway",gateway,15) || !read.text("subnet",subnet,15) ||
      !read.text("dns1",dns1,15) || !read.text("dns2",dns2,15))return false;
  if(!read.supplied) {result=F("No Ethernet configuration fields supplied");return false;}
  const bool changed=address!=candidate.address().toString() || gateway!=candidate.gateway().toString() ||
      subnet!=candidate.subnet().toString() || dns1!=candidate.dns1().toString() || dns2!=candidate.dns2().toString();
  if(!candidate.setAddressing(dhcp&&!changed,address,gateway,subnet,dns1,dns2) ||
      (dhcp&&changed&&!candidate.setAddressing(true))) {result=candidate.lastError();return false;}
  if(!ConfigurationManagement::acknowledgement(result))return false;
  if(!candidate.save()) {result=candidate.lastError();return false;}
  ethernetConfig=candidate;
  ethernetApplyOnly=true;networkApplyAt=millis()+1000U;networkApplyPending=true;
  return true;
#else
  (void)patch;result=F("Ethernet is not compiled into this firmware");return false;
#endif
}
