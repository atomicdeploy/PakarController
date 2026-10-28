#pragma once
#if PAKAR_SCP_ENABLED || PAKAR_SFTP_ENABLED || PAKAR_FTP_ENABLED
#include "TransferFiles.h"
// Exclusive upload lease participates in existing OTA/network/config exclusion.
// All calls and cleanup run on the application loop, never crypto/AsyncTCP tasks.
uint32_t fileTransferLease=0;
uint32_t fileTransferTouched=0;
bool fileTransferCancelPending=false;
void serviceFileTransferCleanup(){
  if(!fileTransferLease)return;
  if(!fileTransferCancelPending&&uint32_t(millis()-fileTransferTouched)<=60000U)return;
  JsonDocument q,r;q["action"]="cancel";q["token"]=fileTransferLease;
  // A transient LED flash fence must not lose the cleanup owner or strand OTA.
  if(!deserializeJson(r,managedUserFileRequest(q.as<JsonObjectConst>(),false))&&
      (r["accepted"]==true||r["error"]=="transfer_expired_or_not_owned")){
    fileTransferLease=0;fileTransferCancelPending=false;--artifactReaders;
  }else fileTransferCancelPending=true;
}
String transferFileRequest(JsonObjectConst body,bool stream){
  const String action=body["action"]|"";
  if(stream&&action=="begin"){
    if(!maintenanceApiIdle())return maintenanceApiError("maintenance_busy");
    ++artifactReaders;
    String result=managedUserFileRequest(body,true);JsonDocument reply;
    if(deserializeJson(reply,result)||reply["accepted"]!=true||!reply["token"].is<uint32_t>()){--artifactReaders;return result;}
    fileTransferLease=reply["token"];fileTransferTouched=millis();fileTransferCancelPending=false;return result;
  }
  if(!fileTransferLease&&!maintenanceApiIdle())return maintenanceApiError("maintenance_busy");
  const bool owned=fileTransferLease&&body["token"].is<uint32_t>()&&body["token"].as<uint32_t>()==fileTransferLease;
  if(owned)fileTransferTouched=millis();
  String result=managedUserFileRequest(body,false);
  if(action=="cancel"&&owned){fileTransferCancelPending=true;serviceFileTransferCleanup();}
  return result;
}
using ManagedTransferFiles=TransferFiles<decltype(&transferFileRequest)>;
#endif
