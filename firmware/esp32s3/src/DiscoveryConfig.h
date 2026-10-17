#pragma once
#include <BuildInfo.h>

// Pakar identity only: no PU weighing routes or unimplemented SOAP/GENA service.
#define FW_UPNP_DEVICE_TYPE "urn:schemas-upnp-org:device:Basic:1"
#define FW_UPNP_SERVICE_TYPE ""
#define FW_PRODUCT_MODEL_NAME "PakarController"
#define FW_MANUFACTURER_NAME "AtomicDeploy"
#define FW_UPNP_SCHEMA_PATH "description.xml"
#define FW_UPNP_CACHE_MAX_AGE_SECONDS 1800
#define FW_UPNP_MULTICAST_TTL 4
#define FW_UPNP_SERVER_PRODUCT "PakarController"
#define FW_UPNP_SERVER_VERSION BoardCore::BuildInfo::kVersion
