#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
bool FirmwareTransferHasAuthorization(AsyncWebServerRequest* request);
bool FirmwareTransferAuthorize(AsyncWebServerRequest* request);
void RegisterFirmwareTransferRoutes(AsyncWebServer* server,
    std::function<bool(AsyncWebServerRequest*)> authorize);
