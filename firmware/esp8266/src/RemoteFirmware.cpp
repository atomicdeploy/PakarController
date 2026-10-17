#include "RemoteFirmware.h"
#include "CloudServices.h"
#include <BuildInfo.h>
#include <time.h>
#include <stdio.h>

namespace {
constexpr char kManifest[]="/remote/manifest.json";
constexpr char kJournal[]="/remote/job.json";
constexpr char kEsp[]="/remote/esp.bin";
constexpr char kAvr[]="/remote/avr.bin";
String origin(const String &url) {
  const int start=url.indexOf("://")+3,end=url.indexOf('/',start);
  String result=url.substring(0,end); result.toLowerCase(); return result;
}
String hashFile(File &f) {
  FirmwarePlatform::Sha256 c; f.seek(0,SeekSet);
  uint8_t block[512];
  while(f.available()) {
    const int n=f.read(block,sizeof(block)); if(n<=0) return String();
    c.add(block,n); yield();
  }
  return c.hex();
}
}
const char *RemoteFirmware::phase() const {
  const char *names[]={"idle","manifest","download_esp","download_avr","ready",
      "prepare","program_avr","program_esp","awaiting_reboot","verify_pair",
      "release_pair","complete","failed","cancelled"};
  return names[phase_];
}
bool RemoteFirmware::busy() const {
  return phase_==Manifest || phase_==DownloadEsp || phase_==DownloadAvr ||
      applying() || phase_==VerifyPair;
}
bool RemoteFirmware::applying() const {
  return phase_==PrepareApply || phase_==ProgramAvr || phase_==ProgramEsp ||
      phase_==Rebooting || phase_==ReleasePair;
}
String RemoteFirmware::digest(const FirmwarePlatform::Sha256 &c) {
  return c.hex();
}
void RemoteFirmware::close() {
  if(file_) file_.close();
#if defined(ARDUINO_ARCH_ESP32)
  espStage_.abort();downloadingEspStage_=false;
#endif
  http_.end();
  // S3 HTTPClient::end leaves its client pointer set when already disconnected.
  // Release TLS resources, but keep that object alive until begin rebinds it.
  if(socket_) socket_->stop();
  opened_=false;
}
void RemoteFirmware::fail(const char *reason) {
  close(); bearer_=String();error_=reason; phase_=Failed;
  // No retry, no automatic rollback and no automatic Prog release after uncertainty.
  save();
}
String RemoteFirmware::json() const {
  JsonDocument d;
  d[F("job_id")]=id_;d[F("boot_id")]=boot_;d[F("phase")]=phase();
  d[F("targets")]=plan_.targets;d[F("busy")]=busy();d[F("cancellable")]=(busy()||phase_==Ready)&&!applying()&&phase_!=VerifyPair;
  d[F("received_bytes")]=transfer_.received;d[F("expected_bytes")]=transfer_.expected;
  d[F("manifest_sha256")]=manifestSha_;d[F("error")]=error_;
  d[F("esp")][F("size")]=plan_.esp.size;d[F("esp")][F("sha256")]=plan_.esp.sha;
  d[F("esp")][F("version")]=plan_.esp.version;
  d[F("avr")][F("size")]=plan_.avr.size;d[F("avr")][F("sha256")]=plan_.avr.sha;
  d[F("avr")][F("version")]=plan_.avr.version;
  d[F("atomic_pair")]=false;d[F("automatic_retry")]=false;d[F("eeprom_policy")]=F("preserve-and-verify");
#if defined(ARDUINO_ARCH_ESP32)
  d[F("esp_staging")]=F("inactive-ota-slot");
#else
  d[F("esp_staging")]=F("filesystem");
#endif
  String out;serializeJson(d,out);return out;
}
bool RemoteFirmware::save() {
  JsonDocument d;
  d[F("id")]=id_;d[F("phase")]=static_cast<uint8_t>(phase_);
  d[F("targets")]=plan_.targets;d[F("manifest_sha256")]=manifestSha_;
  d[F("downgrade")]=downgrade_;d[F("error")]=error_;
  d[F("esp_version")]=plan_.esp.version;d[F("esp_sha")]=plan_.esp.sha;d[F("esp_size")]=plan_.esp.size;
  d[F("avr_version")]=plan_.avr.version;d[F("avr_sha")]=plan_.avr.sha;d[F("avr_size")]=plan_.avr.size;
#if defined(ARDUINO_ARCH_ESP32)
  d[F("esp_stage_address")]=espStage_.address();
#endif
  File f=LittleFS.open("/remote/job.pending","w"); if(!f) return false;
  const size_t expected=measureJson(d),written=serializeJson(d,f);
  f.flush();f.close();
  File check=LittleFS.open("/remote/job.pending","r"); JsonDocument verified;
  const bool ok=check && check.size()==expected && !deserializeJson(verified,check) &&
      verified[F("id")]==id_ && verified[F("phase")]==static_cast<uint8_t>(phase_);
  if(check) check.close();
  return written==expected && ok && LittleFS.rename("/remote/job.pending",kJournal);
}
void RemoteFirmware::begin(const String &boot,Prepare p,Program program,Finish finish) {
  boot_=boot;prepare_=p;program_=program;finish_=finish;
  if(boot_.isEmpty()) {phase_=Failed;error_=F("Boot entropy unavailable; remote apply disabled");return;}
  LittleFS.mkdir("/remote");
  File f=LittleFS.open(kJournal,"r");
  if(!f) return;
  if(f.size()>1024) { f.close();error_=F("Invalid remote journal; preserved for inspection");phase_=Failed;return; }
  JsonDocument d;
  if(deserializeJson(d,f) || !d.is<JsonObject>() || !d[F("id")].is<uint32_t>() ||
      !d[F("phase")].is<unsigned>() || d[F("phase")].as<unsigned>()>Cancelled ||
      !d[F("targets")].is<unsigned>() || d[F("targets")].as<unsigned>()<1 ||
      d[F("targets")].as<unsigned>()>3) {
    f.close();error_=F("Invalid remote journal; preserved for inspection");phase_=Failed;return;
  }
  f.close();id_=d[F("id")];plan_.targets=d[F("targets")];downgrade_=d[F("downgrade")]|false;
  manifestSha_=d[F("manifest_sha256")]|"";error_=d[F("error")]|"";
  phase_=static_cast<Phase>(d[F("phase")].as<unsigned>());
  plan_.esp.version=d[F("esp_version")]|"";plan_.esp.sha=d[F("esp_sha")]|"";plan_.esp.size=d[F("esp_size")]|0;
  plan_.avr.version=d[F("avr_version")]|"";plan_.avr.sha=d[F("avr_sha")]|"";plan_.avr.size=d[F("avr_size")]|0;
  if(phase_==Ready || phase_==Rebooting || phase_==VerifyPair) {
    if(!parseManifest()) { fail(F("Persisted manifest failed validation"));return; }
#if defined(ARDUINO_ARCH_ESP32)
    if(phase_==Ready && (plan_.targets&RemotePolicy::Esp) &&
        (!d[F("esp_stage_address")].is<uint32_t>() ||
         !espStage_.resume(d[F("esp_stage_address")],plan_.esp.size) || !validateEspStage())) {
      fail(F("Persisted inactive OTA stage failed validation"));return;
    }
#endif
    if(phase_!=Ready) { phase_=VerifyPair;verifyAt_=millis();verifyBoot(); }
  } else if(busy()) fail(F("Interrupted update; inspect Prog and backups. No operation was replayed"));
}
bool RemoteFirmware::queue(const String &url,const String &sha,uint8_t targets,
                           bool applyNow,bool downgrade,const String &bearer) {
  if(boot_.isEmpty() || busy() || id_==UINT32_MAX || targets<1 || targets>3 ||
      !RemotePolicy::url(url.c_str(),PAKAR_ALLOW_HTTP_FIRMWARE) ||
      !RemotePolicy::hash(sha.c_str()) || bearer.length()>256) {
    error_=F("Invalid endpoint/hash/target or an update is active"); return false;
  }
  for(size_t i=0;i<bearer.length();++i)
    if(bearer[i]<33 || bearer[i]>126) {error_=F("Invalid bearer token");return false;}
  // Signed-query URLs and bearer credentials are never emitted in telemetry/journal.
  close();++id_;plan_=Plan{};plan_.targets=targets;
  manifestSha_=sha;bearer_=bearer;credentialOrigin_=origin(url);autoApply_=applyNow;downgrade_=downgrade;
  prepareDone_=false;error_=String();startedAt_=millis();phase_=Manifest;
  transfer_={}; LittleFS.mkdir("/remote");
  // Endpoint stays only in memory until the manifest download begins.
  plan_.esp.url=url;
  if(!save()) {fail(F("Cannot persist update ownership"));return false;}
  return true;
}
bool RemoteFirmware::apply(uint32_t id,const String &boot,const String &sha) {
  if(phase_!=Ready || id!=id_ || boot!=boot_ || sha!=manifestSha_) {
    error_=F("Stale/incorrect job identity or images not ready");return false;
  }
  phase_=PrepareApply;error_=String();
  if(!save()) {fail(F("Cannot journal apply intent"));return false;} return true;
}
bool RemoteFirmware::cancel(uint32_t id,const String &boot) {
  if(id!=id_ || boot!=boot_ || applying() || phase_==VerifyPair ||
      !(busy() || phase_==Ready)) {error_=F("Job not cancellable");return false;}
  close();bearer_=String();phase_=Cancelled;error_=String();return save();
}
bool RemoteFirmware::openDownload(const String &url,uint32_t size,const String &sha,const char *path) {
  error_=String();
  auto reject=[this](const char *reason){error_=reason;return false;};
  if(!RemotePolicy::url(url.c_str(),PAKAR_ALLOW_HTTP_FIRMWARE)) return reject("Download URL policy rejected");
  std::unique_ptr<WiFiClient> nextSocket;
  if(url.startsWith("https://")) {
    if(time(nullptr)<1700000000) return reject("Download requires a valid TLS clock");
    if(ESP.getFreeHeap()<18000) return reject("Download TLS heap below safety reserve");
#if defined(ARDUINO_ARCH_ESP32)
    auto *secure=new WiFiClientSecure;
    secure->setCACert(pakarTrustAnchorPem());
    secure->setTimeout(5000);
#else
    auto *secure=new BearSSL::WiFiClientSecure;
    secure->setTrustAnchors(&pakarTrustAnchor());
    // Prefer general TLS records if sufficient contiguous heap is available.
    // Otherwise the publisher must support 4 KiB MFLN/small TLS records; an
    // oversized TLS record fails the download before any erase, never insecure.
    const bool fullRecords=ESP.getFreeHeap()>=34000 && ESP.getMaxFreeBlockSize()>=26000;
    secure->setBufferSizes(fullRecords?16384:4096,512);secure->setTimeout(5000);
#endif
    nextSocket.reset(secure);
  } else nextSocket.reset(new WiFiClient);
  // Bind before replacing the old socket or calling setters that inspect it.
  const bool bound=http_.begin(*nextSocket,url);
  socket_=std::move(nextSocket);
  if(!bound) return reject("Download HTTP client binding failed");
  http_.setTimeout(5000);http_.setReuse(false);http_.useHTTP10(true);
  // No redirects: credentials cannot leak to another authority. Publish direct URLs.
  http_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char *headers[]={"Content-Encoding","Transfer-Encoding"};
  http_.collectHeaders(headers,2);
  if(bearer_.length() && origin(url)==credentialOrigin_)
    http_.addHeader("Authorization",String("Bearer ")+bearer_);
  http_.addHeader("Accept-Encoding","identity");
  const int code=http_.GET(),length=http_.getSize();
  if(code!=200) {
    char message[96]{};snprintf(message,sizeof(message),"Download HTTP status %d",code);error_=message;
#if defined(ARDUINO_ARCH_ESP32)
    if(code<0 && url.startsWith("https://")) {
      char unusedReason[96]{};
      const int tls=static_cast<WiFiClientSecure*>(socket_.get())->lastError(unusedReason,sizeof(unusedReason));
      snprintf(message,sizeof(message),"; TLS error %d",tls);error_+=message;
    }
#endif
    // Numeric SDK errors only: never expose URLs, bearer tokens or response bodies.
    return false;
  }
  if(length<1 || (size && static_cast<uint32_t>(length)!=size) || (!size && length>4096))
    return reject("Download Content-Length missing or mismatched");
  if(http_.header("Content-Encoding").length()) return reject("Download Content-Encoding is not identity");
  if(http_.header("Transfer-Encoding").length()) return reject("Download Transfer-Encoding is not supported");
  transfer_={static_cast<uint32_t>(length),0};expectedSha_=sha;
#if defined(ARDUINO_ARCH_ESP32)
  downloadingEspStage_=!strcmp(path,kEsp);
  if(downloadingEspStage_) {if(!espStage_.begin(size)) return reject("Download inactive OTA stage could not begin");}
  else
#endif
  {file_=LittleFS.open(path,"w");if(!file_) return reject("Download filesystem stage could not open");}
  sha_.begin();lastDataAt_=millis();opened_=true;return true;
}
int RemoteFirmware::download() {
  uint8_t block[1024];auto *s=http_.getStreamPtr();
  const int available=s?s->available():0;
  if(available>0) {
    const size_t n=min<size_t>(sizeof(block),min<uint32_t>(available,transfer_.expected-transfer_.received));
    if(!n) return -1;
    const int read=s->read(block,n);
    if(read<=0 || !transfer_.append(read)) return -1;
#if defined(ARDUINO_ARCH_ESP32)
    if(downloadingEspStage_) {if(!espStage_.write(block,read)) return -1;}
    else
#endif
    if(file_.write(block,read)!=static_cast<size_t>(read)) return -1;
    sha_.add(block,read);lastDataAt_=millis();
  }
  if(transfer_.received==transfer_.expected) {
    if(s && s->available()>0) return -1;
    bool ok=transfer_.complete(digest(sha_)==expectedSha_);
#if defined(ARDUINO_ARCH_ESP32)
    if(downloadingEspStage_) {ok=ok && espStage_.finish();}
    else
#endif
    file_.flush();
    close();return ok?1:-1;
  }
  if(!http_.connected() && !available) return -1;
  if(static_cast<uint32_t>(millis()-lastDataAt_)>20000 ||
      static_cast<uint32_t>(millis()-startedAt_)>300000) return -1;
  return 0;
}
bool RemoteFirmware::parseManifest() {
  plan_.allowDowngrade=downgrade_;
  File f=LittleFS.open(kManifest,"r");
  if(!f || f.size()>4096 || hashFile(f)!=manifestSha_) {if(f)f.close();return false;}
  f.seek(0,SeekSet);JsonDocument d;const bool parsed=!deserializeJson(d,f);f.close();
  if(!parsed || !d.is<JsonObject>() || d[F("product")]!="PakarController" ||
      d[F("profile")]!=FirmwarePlatform::profile()) return false;
#if defined(ARDUINO_ARCH_ESP32)
  if(d[F("link_mode")]!="buffered") return false;
#elif PAKAR_USE_BUSY_WIRE
  if(d[F("link_mode")]!="busy-wire") return false;
#else
  if(d[F("link_mode")]!="polling") return false;
#endif
  auto read=[&](const char *key,Component &c,bool esp) {
    const JsonObjectConst o=d[key].as<JsonObjectConst>();
    if(o.isNull() || !o[F("url")].is<const char*>() || !o[F("sha256")].is<const char*>() ||
        !o[F("version")].is<const char*>() || !o[F("size")].is<uint32_t>()) return false;
    c.url=o[F("url")].as<String>();c.sha=o[F("sha256")].as<String>();
    c.version=o[F("version")].as<String>();c.size=o[F("size")];
    uint16_t v[3];
    if(!RemotePolicy::url(c.url.c_str(),PAKAR_ALLOW_HTTP_FIRMWARE) ||
       !RemotePolicy::hash(c.sha.c_str()) || !RemotePolicy::version(c.version.c_str(),v) ||
       (!downgrade_ && RemotePolicy::compare(c.version.c_str(),BoardCore::BuildInfo::kVersion)<0)) return false;
    if(esp) return o[F("model")]==FirmwarePlatform::model() && c.size>=24 &&
        FirmwarePlatform::otaCapacity() && c.size<FirmwarePlatform::otaCapacity();
    return o[F("model")]=="atmega328p" && o[F("signature")]=="1E950F" &&
        o[F("bootloader_included")]==true && c.size==32768;
  };
  if((plan_.targets&RemotePolicy::Esp) && !read("esp",plan_.esp,true)) return false;
  if((plan_.targets&RemotePolicy::Avr) && !read("avr",plan_.avr,false)) return false;
  if(plan_.targets==RemotePolicy::All && plan_.esp.version!=plan_.avr.version) return false;
  return true;
}
bool RemoteFirmware::validateImage(const Component &c,const char *path,bool esp) {
#if defined(ARDUINO_ARCH_ESP32)
  if(esp) return c.size==espStage_.size() && validateEspStage();
#endif
  File f=LittleFS.open(path,"r");
  if(!f || f.size()!=c.size || hashFile(f)!=c.sha) {if(f)f.close();return false;}
  uint8_t p[24];f.seek(0,SeekSet);
  bool ok=f.read(p,sizeof(p))==sizeof(p) &&
      (esp?RemotePolicy::espHeader(p,sizeof(p)):RemotePolicy::avrVector(p,sizeof(p)));
  // ESP updater rewrites a differing flash-mode byte. Reject that before AVR
  // erase rather than accepting a post-reboot SHA mismatch caused by patching.
  if(esp) {
#if defined(ARDUINO_ARCH_ESP32)
    esp_image_header_t header{};
    memcpy(&header,p,sizeof(header));
    ok &= header.chip_id==ESP_CHIP_ID_ESP32S3 && header.hash_appended==1;
    const uint16_t revision=ESP.getChipRevision();
    ok &= revision>=header.min_chip_rev_full &&
        (header.max_chip_rev_full==0 || header.max_chip_rev_full==UINT16_MAX ||
         revision<=header.max_chip_rev_full);
#else
    ok &= p[2]==static_cast<uint8_t>(FirmwarePlatform::flashChipMode());
#endif
  }
  if(!esp) {
    // Full raw flash must contain a bootloader, not an application padded with FF.
    f.seek(32768-384,SeekSet);bool nonblank=false;
    while(f.available()) nonblank|=f.read()!=0xFF;
    ok&=nonblank;
  }
  f.close();return ok;
}
bool RemoteFirmware::validateEspStage() {
#if defined(ARDUINO_ARCH_ESP32)
  if(espStage_.size()!=plan_.esp.size) return false;
  esp_image_header_t header{};
  if(!espStage_.read(0,reinterpret_cast<uint8_t *>(&header),sizeof(header)) ||
     header.magic!=0xe9 || header.chip_id!=ESP_CHIP_ID_ESP32S3 || header.hash_appended!=1) return false;
  const uint16_t revision=ESP.getChipRevision();
  if(revision<header.min_chip_rev_full ||
     (header.max_chip_rev_full && header.max_chip_rev_full!=UINT16_MAX && revision>header.max_chip_rev_full)) return false;
  FirmwarePlatform::Sha256 c;uint8_t block[1024];
  for(uint32_t offset=0;offset<plan_.esp.size;offset+=sizeof(block)) {
    const size_t bytes=min<uint32_t>(sizeof(block),plan_.esp.size-offset);
    if(!espStage_.read(offset,block,bytes)) return false;
    c.add(block,bytes);yield();
  }
  return c.hex()==plan_.esp.sha;
#else
  return false;
#endif
}
void RemoteFirmware::verifyBoot() {
  if(!(plan_.targets&RemotePolicy::Esp)) return;
  if(plan_.esp.version!=BoardCore::BuildInfo::kVersion || !FirmwarePlatform::runningIntegrity()) {
    fail(F("ESP running version/integrity did not match staged update"));return;
  }
  FirmwarePlatform::Sha256 c;alignas(4) uint8_t block[512];
  for(uint32_t offset=0;offset<plan_.esp.size;offset+=sizeof(block)) {
    const size_t n=min<uint32_t>(sizeof(block),plan_.esp.size-offset);
    if(!FirmwarePlatform::readRunning(offset,block,n)) {fail(F("Cannot verify running ESP flash"));return;}
    c.add(block,n);yield();
  }
  if(digest(c)!=plan_.esp.sha) fail(F("Running ESP SHA-256 mismatch; Prog remains armed"));
}
bool RemoteFirmware::installEsp() {
#if defined(ARDUINO_ARCH_ESP32)
  if(!validateEspStage()) return false;
  // A durable intent precedes selecting the already-validated inactive slot.
  // Power loss before/after selection is verified on boot, never auto-replayed.
  phase_=Rebooting;
  if(!save() || !espStage_.selectForBoot()) return false;
  rebootAt_=millis()+2000;return true;
#else
  File f=LittleFS.open(kEsp,"r");if(!f) return false;
  const uint32_t maximum=FirmwarePlatform::otaCapacity();
  // Leave a gap: end(false) must always abort, never commit a full unchecked stream.
  if(!maximum || plan_.esp.size>=maximum) {f.close();return false;}
#if !defined(ARDUINO_ARCH_ESP32)
  Update.runAsync(true);
#endif
  if(!Update.begin(maximum,U_FLASH)) {f.close();return false;}
  FirmwarePlatform::Sha256 c;uint8_t block[1024];uint32_t written=0;
  while(f.available()) {
    const int n=f.read(block,sizeof(block));
    if(n<=0 || Update.write(block,n)!=static_cast<size_t>(n)) {f.close();FirmwarePlatform::abortUpdate();return false;}
    written+=n;c.add(block,n);
    transfer_={plan_.esp.size,written};yield();
  }
  f.close();
  if(written!=plan_.esp.size || digest(c)!=plan_.esp.sha) {FirmwarePlatform::abortUpdate();return false;}
  phase_=Rebooting;
  if(!save()) {FirmwarePlatform::abortUpdate();return false;}
  if(!Update.end(true)) return false;
  rebootAt_=millis()+2000;return true;
#endif
}
void RemoteFirmware::service() {
  if(phase_==Rebooting) {
    if(static_cast<int32_t>(millis()-rebootAt_)>=0) ESP.restart();
    return;
  }
  if(phase_==VerifyPair) {
    String reason;
    const int8_t ready=finish_?finish_(plan_,false,reason):-1;
    if(ready<0) {fail(reason.length()?reason.c_str():"Pair verification failed");return;}
    if(ready==1) {
      // Durable intent precedes the one hardware mutation. A reboot in this
      // state is uncertainty, not permission to replay release.
      phase_=ReleasePair;
      if(!save()) {fail(F("Cannot journal Prog release intent"));return;}
      if(finish_(plan_,true,reason)!=1) {fail(reason.length()?reason.c_str():"Prog release outcome unknown");return;}
      phase_=Complete;error_=String();
      if(!save()) {phase_=Failed;error_=F("Pair verified but completion journal failed");}
      return;
    }
    if(static_cast<uint32_t>(millis()-verifyAt_)>15000) fail(reason.length()?reason.c_str():"Pair verification timed out; inspect Prog");
    return;
  }
  if(phase_==Manifest || phase_==DownloadEsp || phase_==DownloadAvr) {
    if(!opened_) {
      const bool manifest=phase_==Manifest,esp=phase_==DownloadEsp;
      const Component &c=esp?plan_.esp:plan_.avr;
      if(!openDownload(manifest?plan_.esp.url:c.url,manifest?0:c.size,
          manifest?manifestSha_:c.sha,manifest?kManifest:esp?kEsp:kAvr)) {
        const String reason=error_;
        fail(reason.length()?reason.c_str():"Download rejected before transfer");return;
      }
    }
    const int result=download();
    if(result<0) {fail(F("Download incomplete, oversized, timed out or SHA-256 mismatched"));return;}
    if(!result) return;
    if(phase_==Manifest) {
      if(!parseManifest()) {fail(F("Manifest identity/profile/version/target validation failed"));return;}
      const uint32_t need=
#if !defined(ARDUINO_ARCH_ESP32)
          ((plan_.targets&1)?plan_.esp.size:0)+
#endif
          ((plan_.targets&2)?plan_.avr.size:0)+98304;
      if(FirmwarePlatform::freeStorage()<need) {fail(F("Insufficient staging and recovery storage"));return;}
      phase_=(plan_.targets&1)?DownloadEsp:DownloadAvr;
    } else {
      const bool esp=phase_==DownloadEsp;
      if(!validateImage(esp?plan_.esp:plan_.avr,esp?kEsp:kAvr,esp)) {fail(F("Staged image/header/bootloader failed validation"));return;}
      phase_=esp && (plan_.targets&2)?DownloadAvr:Ready;
      if(phase_==Ready) {bearer_=String(); if(autoApply_) phase_=PrepareApply;}
    }
    if(!save()) fail(F("Cannot persist validated stage"));
    return;
  }
  if(phase_==PrepareApply) {
    if(((plan_.targets&1)&&!validateImage(plan_.esp,kEsp,true)) ||
       ((plan_.targets&2)&&!validateImage(plan_.avr,kAvr,false))) {fail(F("Staging changed; no board was written"));return;}
    if((plan_.targets&1) && (!FirmwarePlatform::otaCapacity() ||
       plan_.esp.size>=FirmwarePlatform::otaCapacity())) {
      fail(F("ESP OTA capacity insufficient; no AVR erase performed"));return;
    }
    String reason;
    if(!prepare_ || !prepare_(plan_,reason)) {fail(reason.length()?reason.c_str():"Prog/safety preparation failed");return;}
    prepareDone_=true;phase_=(plan_.targets&2)?ProgramAvr:ProgramEsp;
    if(!save()) fail(F("Cannot journal prepared state; Prog remains armed"));
    return;
  }
  if(phase_==ProgramAvr) {
    String reason;
    if(!prepareDone_ || !program_ || !program_(kAvr,plan_.avr.version,reason)) {
      fail(reason.length()?reason.c_str():"AVR programming/verification failed; Prog remains armed");return;
    }
    phase_=(plan_.targets&1)?ProgramEsp:VerifyPair;verifyAt_=millis();
    if(!save()) fail(F("Cannot journal AVR completion; Prog remains armed"));
    return;
  }
  if(phase_==ProgramEsp && !installEsp()) fail(F("ESP commit failed; inspect recovery backups and Prog"));
}
