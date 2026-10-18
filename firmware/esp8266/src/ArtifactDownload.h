#pragma once
#include "FirmwareRange.h"
// Allowlisted firmware/recovery artifacts only. Never serve arbitrary LittleFS
// paths (Wi-Fi/auth/cloud settings and signed endpoint URLs are private).
struct ArtifactReadLease {
  File file;unsigned &readers;
  ArtifactReadLease(File f,unsigned &count):file(f),readers(count){++readers;}
  ~ArtifactReadLease(){file.close();--readers;}
};
struct ArtifactCounterLease {
  unsigned &readers;
  explicit ArtifactCounterLease(unsigned &count):readers(count){++readers;}
  ~ArtifactCounterLease(){--readers;}
};
void registerArtifactDownload() {
  server.native().on("/file/download",HTTP_GET|HTTP_HEAD,[](AsyncWebServerRequest *request) {
    if(!webAccess.allowed(request) || !WebAccess::sameOrigin(request)){request->send(401);return;}
    if(urbootBusy.get() || remoteFirmware.busy() || otaActive || otaRebootAt || avrUploadOwner || server.processing() || artifactReaders>=4) {
      request->send(409,"text/plain","Maintenance/storage active");return;
    }
    const String kind=request->arg("artifact");
#if defined(ARDUINO_ARCH_ESP32)
    if(kind=="esp-staged") {
      const uint32_t total=remoteFirmware.stagedEspSize();
      if(!total){request->send(404,"text/plain","No verified ESP stage ready");return;}
      FirmwareTransferRange::FirmwareRange range{0,total-1,false};
      if(request->hasHeader("Range") && !FirmwareTransferRange::parseRange(request->getHeader("Range")->value(),total,range)) {
        auto *r=request->beginResponse(416);r->addHeader("Content-Range",String("bytes */")+total);request->send(r);return;
      }
      const uint32_t length=range.end-range.start+1;AsyncWebServerResponse *r=nullptr;
      if(request->method()==HTTP_HEAD){r=request->beginResponse(range.partial?206:200,"application/octet-stream","");r->setContentLength(length);}
      else {
        auto lease=std::make_shared<ArtifactCounterLease>(artifactReaders);
        r=request->beginResponse("application/octet-stream",length,
          [lease,start=range.start,length](uint8_t *out,size_t maximum,size_t offset)->size_t {
            if(offset>=length)return 0;const size_t bytes=min<size_t>(maximum,length-offset);
            return remoteFirmware.readStagedEsp(start+offset,out,bytes)?bytes:0;
          });
        if(range.partial)r->setCode(206);
      }
      r->addHeader("Cache-Control","no-store");r->addHeader("Accept-Ranges","bytes");
      r->addHeader("X-Firmware-Size",String(total));
      r->addHeader("Content-Disposition","attachment; filename=\"PakarController-esp-staged.bin\"");
      if(range.partial)r->addHeader("Content-Range",String("bytes ")+range.start+"-"+range.end+"/"+total);
      request->send(r);return;
    }
#endif
    if(kind=="esp-running") {
      auto *r=request->beginResponse(307);r->addHeader("Location","/firmware/download");
      r->addHeader("Cache-Control","no-store");request->send(r);return;
    }
    const char *path=nullptr;
    if(kind=="avr-flash")path="/avr/backup/flash.bin";
    else if(kind=="urboot-flash")path="/avr/urboot-backup/flash.bin";
    else if(kind=="urboot-eeprom")path="/avr/urboot-backup/eeprom.bin";
    else if(kind=="avr-eeprom")path="/avr/backup/eeprom.bin";
    else if(kind=="esp-staged")path="/remote/esp.bin";
    else if(kind=="avr-staged")path="/remote/avr.bin";
    if(!path){request->send(400,"text/plain","artifact: esp-running|avr-flash|avr-eeprom|esp-staged|avr-staged");return;}
    File file=LittleFS.open(path,"r");
    if(!file || !file.size()){if(file)file.close();request->send(404,"text/plain","Artifact unavailable");return;}
    const uint32_t total=file.size();
    FirmwareTransferRange::FirmwareRange range{0,total-1,false};
    if(request->hasHeader("Range") && !FirmwareTransferRange::parseRange(request->getHeader("Range")->value(),total,range)) {
      file.close();auto *r=request->beginResponse(416);
      r->addHeader("Content-Range",String("bytes */")+total);request->send(r);return;
    }
    const uint32_t length=range.end-range.start+1;
    AsyncWebServerResponse *r=nullptr;
    if(request->method()==HTTP_HEAD) {
      file.close();r=request->beginResponse(range.partial?206:200,"application/octet-stream","");
      r->setContentLength(length);
    } else {
      auto lease=std::make_shared<ArtifactReadLease>(file,artifactReaders);
      r=request->beginResponse("application/octet-stream",length,
          [lease,start=range.start,length](uint8_t *out,size_t maximum,size_t offset)->size_t {
        if(offset>=length || !lease->file.seek(start+offset,SeekSet))return 0;
        return lease->file.read(out,min<size_t>(maximum,length-offset));
      });
      if(range.partial)r->setCode(206);
    }
    r->addHeader("Cache-Control","no-store");r->addHeader("Accept-Ranges","bytes");
    r->addHeader("X-Firmware-Size",String(total));
    r->addHeader("Content-Disposition",String("attachment; filename=\"PakarController-")+kind+".bin\"");
    if(range.partial)r->addHeader("Content-Range",String("bytes ")+range.start+"-"+range.end+"/"+total);
    request->send(r);
  });
}
