#pragma once
#include "UrbootProtocol.h"

// Loop-owned, finite operation, separate from the HTTP request lifetime. Reuses
// PCController preflash.go's backup-before-write, source reinspection and explicit
// verification boundaries. A lost response never triggers another reset/write.
String urbootJobJson() {
  JsonDocument d;d["job_id"]=urbootJobId;d["boot_id"]=pcBootId;
  d["method"]=avrMaintenanceMethod;d["phase"]=urbootPhase;d["operation"]=urbootOperation;
  d["busy"]=urbootBusy.get();d["received_bytes"]=urbootDone;d["expected_bytes"]=urbootTotal;
  d["programmed"]=urbootProgrammed;d["verified"]=urbootVerified;
  d["write_attempted"]=avrWriteAttempted;
  d["application_recovered"]=urbootRecovered;d["error"]=urbootError;
  d["sha256"]=urbootImageSha;d["detected"]=urbootDetected;
  d["request_id"]=avrMaintenanceRequest;
  d["supported"]=urbootSupported();d["automatic_retry"]=false;
  d["bootloader_write"]=avrMaintenanceMethod=="isp" && urbootOperation=="program";
  d["eeprom_write"]=avrMaintenanceMethod=="isp" && urbootOperation=="program";
  d["eeprom_policy"]="preserve-and-verify";d["fuse_write"]=false;
  JsonArray methods=d["methods"].to<JsonArray>();
  if(currentFeatures() & BoardCore::Context::Isp)methods.add("isp");
  if(urbootSupported())methods.add("urboot");
  String out;serializeJson(d,out);return out;
}
bool urbootSupported() {
#if defined(ARDUINO_ARCH_ESP32)
  return PAKAR_AVR_LINK_ENABLED && PAKAR_S3_ISP_ELECTRICAL_VERIFIED;
#else
  return false; // SoftwareSerial is not advertised as a reliable Urboot uploader.
#endif
}
#if defined(ARDUINO_ARCH_ESP32)
struct UrbootPort {
  Stream &stream;
  int available(){return stream.available();}int read(){return stream.read();}
  size_t write(const uint8_t *p,size_t n){return stream.write(p,n);}
  uint32_t now(){return millis();}void idle(){delay(1);}
};
// Only bounded selected-owner status/USB reads are serviced here. All mutations
// are inhibited before dispatch; no cloud/TLS reconnect or LED frame transmission
// competes with the raw UART session. Network callbacks never own the UART.
void urbootBackground() {
  hostLink.service();server.service();
  // Drain ingress while the maintenance fence is STILL set. Never retain a
  // user's queued cell/stream command and apply it after programming completes.
  static LoopMessages::Message message;
  if(loopMessages.pop(message)) {
    if(message.opcode==0xfe)grid.discardStaged((1ULL<<63)|message.client);
    else if(message.epoch==server.ingressEpoch() && webAccess.socketAllowed(message.client))
      handleWebSocketMessage(message.client,message.opcode,message.data,message.length);
  }
  if(stream.parsePacket()>0) {
    uint8_t scratch[64];unsigned remaining=4096;
    while(remaining && stream.available()>0) {
      const int count=stream.read(scratch,min<unsigned>(sizeof(scratch),remaining));
      if(count<=0)break;remaining-=count;
    }
  }
  if(uint32_t(millis()-urbootPublishedAt)>=250U) {
    urbootPublishedAt=millis();
    if(webSocket.count())webSocket.textAll(buildStatusJson());
  }
  delay(1);yield();
}
void urbootProgress(const char *phase,uint32_t done,uint32_t total) {
  urbootPhase=phase;urbootDone=done;urbootTotal=total;urbootBackground();
}
bool urbootFileHash(File &file,String &hash) {
  if(!file || !file.seek(0,SeekSet))return false;
  FirmwarePlatform::Sha256 digest;uint8_t bytes[128];const size_t size=file.size();
  for(size_t at=0;at<size;at+=sizeof(bytes)) {
    const size_t count=min<size_t>(sizeof(bytes),size-at);
    if(file.read(bytes,count)!=count)return false;
    digest.add(bytes,count);
  }
  hash=digest.hex();return file.seek(0,SeekSet);
}
bool runUrboot(Stream &stream) {
  UrbootPort port{stream};PakarUrboot::Protocol<UrbootPort> protocol(port);
  if(!protocol.connect()){urbootError=protocol.error();return false;}
  urbootDetected=true;
  if(urbootOperation=="probe") {protocol.leave();return true;}
  const bool program=urbootOperation=="program";
  File image;
  if(program) {
    image=LittleFS.open(kAvrImagePath,"r");String hash;
    if(!image || image.size()!=32768 || !urbootFileHash(image,hash) || hash!=urbootImageSha) {
      urbootError="Staged image changed; no flash write";return false;
    }
  }
  // Distinct backups: never replace the ISP recovery files or an incomplete
  // pending capture with an acknowledged complete backup.
  LittleFS.mkdir("/avr/urboot-backup");
  uint8_t block[128],expected[128];
  for(unsigned memory=0;memory<2;++memory) {
    const char *pending=memory?"/avr/urboot-backup/eeprom.pending":"/avr/urboot-backup/flash.pending";
    const char *committed=memory?"/avr/urboot-backup/eeprom.bin":"/avr/urboot-backup/flash.bin";
    const uint32_t size=memory?1024:32768;
    File backup=LittleFS.open(pending,"w");FirmwarePlatform::Sha256 digest;
    if(!backup){urbootError="Cannot create Urboot recovery backup";return false;}
    for(uint32_t at=0;at<size;at+=128) {
      if(!protocol.read(memory,at,block,128)){urbootError=protocol.error();return false;}
      if(program && !memory && at>=32384) {
        if(!image.seek(at,SeekSet)||image.read(expected,128)!=128||memcmp(block,expected,128)) {
          urbootError="UART cannot replace a different bootloader; use explicit ISP";return false;
        }
      }
      if(backup.write(block,128)!=128){urbootError="Urboot backup write failed; no flash write";return false;}
      digest.add(block,128);urbootProgress(memory?"backup_eeprom":"backup_flash",at+128,size);
    }
    backup.flush();backup.close();backup=LittleFS.open(pending,"r");String stored;
    if(!backup || backup.size()!=size || !urbootFileHash(backup,stored) || stored!=digest.hex()) {
      urbootError="Durable Urboot backup verification failed; no flash write";return false;
    }
    backup.close();
    if(!LittleFS.rename(pending,committed)){urbootError="Urboot backup commit failed; no flash write";return false;}
  }
  if(program) {
    // A complete already-vector-patched image is required. UART never patches
    // unknown vectors, guesses a boot size or changes its protected region.
    if(!image.seek(0,SeekSet)||image.read(block,128)!=128 ||
        block[0]!=0x3f || block[1]!=0xcf || block[2] || block[3] ||
        block[100]!=0x0c || block[101]!=0x94 ||
        (uint32_t(block[102])+256U*block[103])*2U<104 ||
        (uint32_t(block[102])+256U*block[103])*2U>=32384) {
      urbootError="Invalid reviewed Urboot reset/application vector chain; no flash write";return false;
    }
    String hash;
    if(!urbootFileHash(image,hash)||hash!=urbootImageSha){urbootError="Image changed after backup; no flash write";return false;}
    if(commandClock()>=avrMaintenanceDeadline){urbootError="Admission expired before UART programming; no flash write";return false;}
    // Page zero last; keep the known boot entry until every other page is sent.
    for(uint32_t i=1;i<=253;++i) {
      const uint16_t address=i==253?0:uint16_t(i*128);
      if(!image.seek(address,SeekSet)||image.read(block,128)!=128) {urbootError="Staged page unreadable; stopped";return false;}
      avrWriteAttempted=true;
      if(!protocol.program(address,block)){urbootError=protocol.error();return false;}
      urbootProgress("program_avr",i*128,32384);
    }
    urbootProgrammed=true;
    File eeprom=LittleFS.open("/avr/urboot-backup/eeprom.bin","r");
    for(unsigned memory=0;memory<2;++memory) {
      File &reference=memory?eeprom:image;const uint32_t size=memory?1024:32768;
      if(!reference || reference.size()!=size || !reference.seek(0,SeekSet)) {urbootError="Recovery verification source missing";return false;}
      for(uint32_t at=0;at<size;at+=128) {
        if(reference.read(expected,128)!=128 || !protocol.read(memory,at,block,128) || memcmp(block,expected,128)) {
          urbootError="Urboot full flash/EEPROM readback mismatch; Prog retained";return false;
        }
        urbootProgress(memory?"verify_eeprom":"verify_flash",at+128,size);
      }
    }
    urbootVerified=true;
  }
  protocol.leave();return true;
}
#endif
void serviceUrbootJob() {
  if(!urbootQueued)return;
  urbootQueued=false;
#if defined(ARDUINO_ARCH_ESP32)
  bool success=true;urbootPhase="prepare";
  const bool program=urbootOperation=="program";
  if(commandClock()>=avrMaintenanceDeadline) {
    urbootError="Admission expired before target effects";success=false;
  }
  if(success && program) {
    File candidate=LittleFS.open(kAvrImagePath,"r");String hash;uint8_t header[104];
    if(!candidate || candidate.size()!=32768 || !urbootFileHash(candidate,hash) || hash!=urbootImageSha ||
       candidate.read(header,sizeof(header))!=sizeof(header) || header[0]!=0x3f || header[1]!=0xcf ||
       header[2] || header[3] || header[100]!=0x0c || header[101]!=0x94 ||
       (uint32_t(header[102])+256U*header[103])*2U<104 ||
       (uint32_t(header[102])+256U*header[103])*2U>=32384) {
      urbootError="Pinned complete image/vector preflight failed; no Prog or flash write";success=false;
    }
  }
  if(success && commandClock()>=avrMaintenanceDeadline) {
    urbootError="Admission expired before Prog/reset";success=false;
  }
  if(success && program) {
    const uint8_t armed[]={1};
    if(!avrOnline || !avrVersionKnown || !avrPcSupported ||
        !requestAndWaitForAck(kProgrammingState,armed,1)) {
      urbootError="Fresh AVR and durable Prog ACK required; no flash write";success=false;
    } else programmingLatched=true;
  }
  const bool isp=avrMaintenanceMethod=="isp";
  if(success && commandClock()>=avrMaintenanceDeadline) {
    urbootError="Admission expired before reset";success=false;
  }
  Stream *raw=success && !isp?avrUart.acquireRaw():nullptr;
  if(success && !isp && !raw){urbootError="Exclusive AVR UART unavailable";success=false;}
  const bool acted=raw || (success && isp);
  if(success && isp) {
    urbootRawOwned=true;avrOnline=false;avrVersionKnown=false;avrPcSupported=false;
    avrIsp.setServiceCallback(urbootBackground);
    if(urbootOperation=="probe") {
      uint8_t signature[3];success=avrIsp.probe(signature);urbootDetected=success;
    } else {
      urbootPhase="backup_isp";urbootDone=urbootTotal=0;
      success=avrIsp.backup(LittleFS,kBackupDirectory);
      if(success && program) {
        File image=LittleFS.open(kAvrImagePath,"r"),eeprom=LittleFS.open(String(kBackupDirectory)+"/eeprom.bin","r");
        String hash;
        if(!image || image.size()!=32768 || !urbootFileHash(image,hash) || hash!=urbootImageSha || commandClock()>=avrMaintenanceDeadline) {
          urbootError="Staged image changed; ISP erase refused";success=false;
        } else {
          urbootPhase="program_isp";
          avrWriteAttempted=true;
          success=avrIsp.programFlash(image,eeprom);
          urbootProgrammed=urbootVerified=success;
        }
      }
    }
    if(!success && !urbootError.length())urbootError=avrIsp.lastError();
    avrIsp.setServiceCallback(nullptr);
  }
  if(raw) {
    urbootRawOwned=true;avrOnline=false;avrVersionKnown=false;avrPcSupported=false;
    // Same explicit electrical/polarity gate as the existing dedicated ISP.
    digitalWrite(kAvrResetPin,!PAKAR_S3_ISP_RESET_ASSERT_LEVEL);pinMode(kAvrResetPin,OUTPUT);
    digitalWrite(kAvrResetPin,PAKAR_S3_ISP_RESET_ASSERT_LEVEL);delay(25);
    digitalWrite(kAvrResetPin,!PAKAR_S3_ISP_RESET_ASSERT_LEVEL);pinMode(kAvrResetPin,INPUT);delay(10);
    success=runUrboot(*raw);
  }
  if(acted) {
    // UART command timeout/partial write never causes another write or reset.
    // Stop sending and allow the one-second boot WDT to return to application.
    urbootPhase="awaiting_application";const uint32_t start=millis();
    while(uint32_t(millis()-start)<1200U)urbootBackground();
    if(raw)avrUart.releaseRaw();else avrUart.resetFrames();
    avrOnline=false;avrVersionKnown=false;avrPcSupported=false;
    urbootRawOwned=false;
    avrRecoveryHelloPending=true;
    const uint32_t recovering=millis();uint32_t helloAt=0;
    while(uint32_t(millis()-recovering)<3000U && (!avrOnline || !avrVersionKnown || !avrPcSupported)) {
      if(!helloAt || uint32_t(millis()-helloAt)>=250U){
        helloAt=millis();avrRecoveryHelloSequence=sequence++;
        sendToAvr(kHello,avrRecoveryHelloSequence);
      }
      avrLink.service();urbootBackground();
    }
    urbootRecovered=avrOnline && avrVersionKnown && avrPcSupported;
    avrRecoveryHelloPending=false;
    if(!urbootRecovered && success){urbootError="Bootloader transaction ended; application UART not recovered";success=false;}
  }
  if(success && program) {
    RemoteFirmware::Plan plan;plan.targets=2;plan.avr.version=BoardCore::BuildInfo::kVersion;
    String reason;
    if(!urbootVerified || finishRemotePair(plan,true,reason)!=1){urbootError=reason;success=false;}
  }
  urbootPhase=success?"complete":"failed";
#else
  urbootPhase="failed";urbootError="Urboot requires the verified S3 hardware-UART profile";
#endif
#if defined(ARDUINO_ARCH_ESP32)
  // The queue is finite; consume its final pre-release entries without allowing
  // a mutation or rendering a new LED frame under the programmer.
  for(unsigned i=0;i<8;++i)urbootBackground();
#endif
  urbootBusy.set(false);
  server.setMaintenanceFence(false);
  hostLink.discardInput();
  stream.begin(kStreamPort);
}
void registerUrbootRoutes() {
  server.on("/api/avr/maintenance",HTTP_GET,[](){if(authenticate())sendMaintenanceJson(urbootJobJson());});
  server.on("/api/avr/maintenance",HTTP_POST,[](){
    if(!server.request() || !server.request()->getHeader("X-Pakar-Board") ||
        !server.request()->getHeader("X-Pakar-Boot")) {
      server.send(409,"text/plain","Explicit board/boot headers required");return;
    }
    if(!authenticate())return;
    const String method=server.arg("method");
    const String id=server.arg("request_id"),expiry=server.arg("expires_at_ms");
    if(!id.length() || id.length()>40 || !expiry.length() || expiry.length()>16) {
      server.send(400,"text/plain","Unique request_id and board-clock expires_at_ms required");return;
    }
    for(size_t i=0;i<id.length();++i)if(!isalnum(static_cast<unsigned char>(id[i])) && id[i]!='_' && id[i]!='-') {
      server.send(400,"text/plain","Invalid request_id");return;
    }
    uint64_t deadline=0;
    for(size_t i=0;i<expiry.length();++i) {
      if(expiry[i]<'0'||expiry[i]>'9'){server.send(400,"text/plain","Invalid expiry");return;}
      deadline=deadline*10U+expiry[i]-'0';
    }
    const uint64_t now=commandClock();
    if(deadline<=now || deadline-now>60000U){server.send(409,"text/plain","Maintenance admission expired");return;}
    for(uint8_t i=0;i<avrMaintenanceSeenCount;++i)if(avrMaintenanceSeen[i]==id) {
      if(avrMaintenanceRequest==id && avrMaintenanceMethod==method && urbootOperation==server.arg("operation") &&
          urbootImageSha==server.arg("sha256"))sendMaintenanceJson(urbootJobJson());
      else server.send(409,"text/plain","Request already consumed or conflicts; no replay");
      return;
    }
    if(avrMaintenanceSeenCount>=64){server.send(409,"text/plain","Per-boot maintenance receipt budget exhausted");return;}
    if((method!="isp" && method!="urboot") || !urbootSupported()) {
      server.send(503,"text/plain","method=isp|urboot and verified S3 UART/reset profile required");return;
    }
    if(urbootBusy.get() || passiveCaptureActive() || artifactReaders || remoteFirmware.busy() ||
        otaActive || otaRebootAt || avrUploadOwner || networkApplyPending || cloudApplyPending ||
        remoteFirmware.ownsEspStage()) {server.send(409,"text/plain","Another maintenance operation owns the board");return;}
    const String operation=server.arg("operation"),hash=server.arg("sha256");
    if(operation!="probe" && operation!="backup" && operation!="program") {server.send(400,"text/plain","operation=probe|backup|program required");return;}
    if(operation=="program" && (!RemotePolicy::hash(hash.c_str()) || !avrOnline || !avrVersionKnown ||
       !avrPcSupported || programmingLatched)) {server.send(409,"text/plain","Fresh AVR/Prog-clear and exact staged SHA256 required");return;}
    if(urbootJobId==UINT32_MAX){server.send(409,"text/plain","Job counter exhausted");return;}
    ++urbootJobId;avrMaintenanceMethod=method;urbootPhase="queued";urbootOperation=operation;urbootImageSha=hash;
    avrMaintenanceSeen[avrMaintenanceSeenCount++]=id;avrMaintenanceRequest=id;avrMaintenanceDeadline=deadline;
    urbootDone=urbootTotal=0;avrWriteAttempted=urbootProgrammed=urbootVerified=urbootRecovered=urbootDetected=false;urbootError="";
    server.setMaintenanceFence(true);
    // Retire byte-oriented ingress instead of replaying buffered user intent
    // on returning to the application. The UDP listener is rebound afterward.
    stream.stop();
    if(telnetClient)telnetClient.stop();
    telnetEditor.reset();telnetParser.reset();telnetAuthenticated=false;
    urbootBusy.set(true);urbootQueued=true;sendMaintenanceJson(urbootJobJson(),202);
  });
}
