#pragma once
// Included in application namespace. API is shared by physical USB and the
// authenticated, same-origin, peer-fenced HTTP maintenance route.
UserFileStore<FirmwarePlatform::Sha256> userFiles;
#if PAKAR_MICROSD_ENABLED
UserFileStore<FirmwarePlatform::Sha256> sdUserFiles;
#endif
String userFileRequest(JsonObjectConst body){
  const String volume=body["volume"]|"littlefs",action=body["action"]|"";
  auto *store=&userFiles;
  if(volume!="littlefs"){
#if PAKAR_MICROSD_ENABLED
    if(volume=="sd")store=&sdUserFiles;else
#endif
    return F("{\"accepted\":false,\"error\":\"volume_unavailable\"}");
  }
  uint32_t token=0;
  if(action=="begin")SystemPlatform::randomBytes(&token,sizeof(token));
#if defined(ARDUINO_ARCH_ESP32)
  PakarS3::ScopedLedFlashPause pause;
  if(!pause)return F("{\"accepted\":false,\"error\":\"led_flash_guard_busy\"}");
#endif
  return store->request(action,body,millis(),accessConfig.securityRevision(),token);
}
