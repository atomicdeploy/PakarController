#pragma once
#include <ESPmDNS.h>
#include "SsdpDevice.h"
#include "DiscoveryInterfaces.h"
#include "DiscoveryMdns.h"
#include "../../esp8266/src/MndpService.h"
#include "../../esp8266/src/ApplicationIdentity.h"

namespace PakarS3 {
class Discovery {
 public:
  bool begin(const String &hostname, const String &displayName, uint32_t chipId, uint16_t streamPort = 7777) {
    end(); hostname_ = hostname;
    if (!activeDiscoveryInterfaces().count) return false;
    mdnsReady_ = startDiscoveryMdns(MDNS,hostname.c_str(),APP_NAME,APP_ID,
        BoardCore::BuildInfo::kVersion,streamPort);
    ssdp_.setSchemaURL("description.xml"); ssdp_.setHTTPPort(80);
    ssdp_.setName(displayName); ssdp_.setSerialNumber(chipId); ssdp_.setURL("/");
    ssdp_.setDeviceType(FW_UPNP_DEVICE_TYPE);
    ssdp_.setModelName(APP_NAME); ssdp_.setModelNumber(BoardCore::BuildInfo::kVersion);
    ssdp_.setManufacturer("AtomicDeploy"); ssdp_.setManufacturerURL("https://github.com/atomicdeploy");
    ssdp_.setModelURL("https://github.com/atomicdeploy/PakarController");
    char uuid[37];
    snprintf(uuid, sizeof(uuid), "38323636-4558-4dda-9188-cda0e6%06x", chipId & 0x00ffffffU);
    ssdp_.setUUID(uuid); ssdp_.begin(); ssdp_.handleClient();
    mndpReady_ = mndp_.begin();
    return mdnsReady_ || ssdpReady() || mndpReady_;
  }
  void end() {
    if (mdnsReady_) MDNS.end();
    ssdp_.end(); mndp_.stop(); mdnsReady_ = mndpReady_ = false;
  }
  void service(const char *boardId = "") {
    ssdp_.handleClient(); mndp_.service(hostname_.c_str(), boardId);
  }
  bool mdnsReady() const { return mdnsReady_; }
  bool ssdpReady() const { return ssdp_.running(); }
  bool mndpReady() const { return mndp_.running(); }
  bool netbiosReady() const { return false; } // Not provided by the installed S3 core.
  String xmlDescription() const {
    // Reuse the donor's complete XML escaping/description rather than maintaining
    // a separate, inconsistent hand-built discovery identity.
    class XmlOutput : public Print {
     public:
      String value;
      size_t write(uint8_t byte) override { value += static_cast<char>(byte); return 1; }
    } output;
    output.value.reserve(2048); ssdp_.schema(output);
    const int body = output.value.indexOf("\r\n\r\n");
    return body >= 0 ? output.value.substring(body + 4) : String();
  }
 private:
  SsdpDevice ssdp_;
  MndpService mndp_;
  String hostname_;
  bool mdnsReady_ = false, mndpReady_ = false;
};
} // namespace PakarS3
