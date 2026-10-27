#pragma once
#include <ESPAsyncWebServer.h>
#include "AsyncHttpService.h"
#include <functional>
#include <memory>
struct FirmwareTransferIdentity {
  uint32_t size=0;
  String md5;
  bool integrityVerified=false;
};
bool FirmwareTransferHasAuthorization(AsyncWebServerRequest* request);
bool FirmwareTransferAuthorize(AsyncWebServerRequest* request);
// Admission runs in loop(), serialized with maintenance mutations. The acquired
// lease must be safe to release from the SDK actor; null denies a payload GET.
void RegisterFirmwareTransferRoutes(AsyncHttpService* server,
    std::function<bool(AsyncWebServerRequest*)> authorize,
    std::function<FirmwareTransferIdentity()> identityProvider={},
    std::function<std::shared_ptr<void>()> acquireReader={});
