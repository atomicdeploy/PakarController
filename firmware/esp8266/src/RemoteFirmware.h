#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "../../esp32s3/src/OtaStage.h"
#else
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#endif
#include "FirmwarePlatform.h"
#include <memory>
#include "RemotePolicy.h"

#ifndef PAKAR_USE_BUSY_WIRE
#define PAKAR_USE_BUSY_WIRE 1
#endif
#ifndef PAKAR_ALLOW_HTTP_FIRMWARE
#define PAKAR_ALLOW_HTTP_FIRMWARE 0
#endif
static_assert(PAKAR_ALLOW_HTTP_FIRMWARE==0 || PAKAR_ALLOW_HTTP_FIRMWARE==1,
              "PAKAR_ALLOW_HTTP_FIRMWARE must be 0 or 1");
class RemoteFirmware {
 public:
  struct Component { String url,sha,version; uint32_t size=0; };
  struct Plan { Component esp,avr; uint8_t targets=3;bool allowDowngrade=false; };
  // prepare arms Prog; program verifies flash/EEPROM and leaves Prog armed.
  // finish verifies the actual pair and releases Prog, including after ESP reboot.
  using Prepare=bool (*)(const Plan&,String&);
  using Program=bool (*)(const char*,const String&,String&);
  // commit=false is read-only readiness; commit=true is ONE Prog release.
  // 0=wait, 1=verified, -1=uncertain/failed; never replay a lost release ACK.
  using Finish=int8_t (*)(const Plan&,bool,String&);
  void begin(const String &boot, Prepare prepare, Program program, Finish finish);
  bool queue(const String &url,const String &sha,uint8_t targets,bool apply,
             bool downgrade,const String &bearer);
  bool apply(uint32_t id,const String &boot,const String &sha);
  bool cancel(uint32_t id,const String &boot);
  void service();
  bool busy() const;
  bool applying() const;
  bool rebooting() const { return phase_==Rebooting; }
  bool ownsEspStage() const { return phase_==Ready && (plan_.targets&RemotePolicy::Esp); }
#if defined(ARDUINO_ARCH_ESP32)
  uint32_t stagedEspSize() const {return ownsEspStage()?espStage_.size():0;}
  bool readStagedEsp(uint32_t offset,uint8_t *out,size_t bytes) const {
    return ownsEspStage() && espStage_.read(offset,out,bytes);
  }
#endif
  String json() const;
  const String &error() const { return error_; }
  const char *phase() const;
 private:
  enum Phase : uint8_t { Idle,Manifest,DownloadEsp,DownloadAvr,Ready,PrepareApply,
      ProgramAvr,ProgramEsp,Rebooting,VerifyPair,ReleasePair,Complete,Failed,Cancelled };
  bool openDownload(const String&,uint32_t,const String&,const char*);
  int download();
  bool parseManifest();
  bool validateImage(const Component&,const char*,bool);
  bool save();
  void fail(const char*);
  void fail(const __FlashStringHelper *s) {const String reason(s);fail(reason.c_str());}
  void close();
  bool installEsp();
  bool validateEspStage();
  void verifyBoot();
  static String digest(const FirmwarePlatform::Sha256&);
  Phase phase_=Idle;
  Plan plan_;
  String boot_,manifestSha_,bearer_,error_,credentialOrigin_;
  uint32_t id_=0,lastDataAt_=0,startedAt_=0,rebootAt_=0,verifyAt_=0;
  bool autoApply_=false,downgrade_=false,opened_=false,prepareDone_=false;
#if defined(ARDUINO_ARCH_ESP32)
  S3OtaStage espStage_;
  bool downloadingEspStage_=false;
#endif
  RemotePolicy::Transfer transfer_;
  String expectedSha_;
  File file_;
  HTTPClient http_;
  std::unique_ptr<WiFiClient> socket_;
  FirmwarePlatform::Sha256 sha_;
  Prepare prepare_=nullptr; Program program_=nullptr; Finish finish_=nullptr;
};
