#pragma once
#include <stdint.h>

namespace PakarS3 {
// PU850's one hostname/service owner, adapted to checked Arduino-ESP32 startup.
// In3.3.12 the const-char addServiceTxt convenience overload discards its bool;
// select the actual checked overload just as that SDK wrapper does internally.
template<class Responder>
bool discoveryTxt(Responder &responder,const char *service,const char *protocol,const char *key,const char *value) {
  return responder.addServiceTxt(const_cast<char*>(service),const_cast<char*>(protocol),
      const_cast<char*>(key),const_cast<char*>(value));
}
template<class Responder>
bool startDiscoveryMdns(Responder &responder,const char *hostname,const char *appName,
    const char *appId,const char *version,uint16_t streamPort) {
  // SDK begin() can fail AFTER mdns_init succeeds (hostname allocation/set).
  // Always retire that partial owner before the existing bounded retry path.
  if(!responder.begin(hostname)){responder.end();return false;}
  const bool complete=responder.addService("http","tcp",80) &&
    discoveryTxt(responder,"http","tcp","path","/") &&
    discoveryTxt(responder,"http","tcp","app",appName) &&
    discoveryTxt(responder,"http","tcp","app_id",appId) &&
    discoveryTxt(responder,"http","tcp","version",version) &&
    responder.addService("pakarcontroller","tcp",80) &&
    discoveryTxt(responder,"pakarcontroller","tcp","api","/api/status") &&
    discoveryTxt(responder,"pakarcontroller","tcp","websocket","/ws") &&
    responder.addService("pakar-stream","udp",streamPort) &&
    responder.addService("telnet","tcp",23);
  if(!complete)responder.end();
  return complete;
}
} // namespace PakarS3
