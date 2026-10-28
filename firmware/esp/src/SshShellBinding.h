#pragma once
#if PAKAR_SSH_ENABLED
#include "SshScpBinding.h"
// Included beside the existing Telnet/USB owners, not on the crypto task.
String sshStatusJson(){
  JsonDocument d;d["built"]=true;d["enabled"]=deviceSsh.enabled();d["port"]=deviceSsh.port();
  d["listening"]=deviceSsh.listening();d["sessions"]=deviceSsh.active()?1:0;d["session_limit"]=1;
  d["authentication_required"]=true;d["authentication"]="password";d["interactive_shell"]=true;
  d["sftp"]=false;d["forwarding"]=false;d["exec"]=false;
  d["scp_download"]=bool(PAKAR_SCP_ENABLED);d["scp_upload"]=false;d["ftp"]=false;
  if(PAKAR_SCP_ENABLED){d["scp_namespace"]="/user/";d["scp_max_file_bytes"]=128*1024;}
  char fingerprint[100];deviceSsh.fingerprint(fingerprint,sizeof(fingerprint));d["host_key_fingerprint"]=fingerprint;d["error"]=deviceSsh.error();
  String out;serializeJson(d,out);return out;
}
bool updateSshConfiguration(JsonObjectConst patch,String &result){
  if(shellBusy()||networkApplyPending||cloudApplyPending){result=F("Device busy; SSH settings unchanged");return false;}
  if(patch.isNull()||!patch.size()||patch.size()>2){result=F("Supply SSH enabled and/or port");return false;}
  bool enabled=deviceSsh.enabled();uint16_t port=deviceSsh.port();
  for(JsonPairConst pair:patch){
    const String key=pair.key().c_str();const auto value=pair.value();
    if(key=="enabled"&&value.is<bool>())enabled=value.as<bool>();
    else if(key=="port"&&value.is<uint16_t>()&&value.as<uint16_t>())port=value.as<uint16_t>();
    else{result=F("Invalid or unknown SSH setting");return false;}
  }
  if(!deviceSsh.configure(enabled,port,shellSettings.port())){result=F("Invalid/reserved port or durable SSH save failed");return false;}
  PakarConfiguration::changes().saved(PakarConfiguration::Shell);result=F("SSH settings saved; active SSH sessions retire on change");return true;
}
void serviceSshShell(){
  deviceSsh.service(accessConfig.ready()&&(NetworkPath::ready()||apStarted)&&!shellBusy(),accessConfig.securityRevision(),accessConfig.username(),accessConfig.password());
  const uint32_t active=deviceSsh.active(),revision=accessConfig.securityRevision();
  if(sshShell&&(!PakarS3::SshPolicy::admitted(sshShellToken,active,sshShell->accessRevision,revision)||!accessConfig.ready())){sshShell.reset();sshShellToken=0;}
  static PakarS3::SshTransport::Packet packet;
  static bool pending=false;static unsigned offset=0;
  unsigned inputBudget=PakarS3::SshPolicy::inputBudget;
  bool terminalInput=false;
  for(unsigned events=0;events<4&&inputBudget;++events){
    if(!pending){if(!deviceSsh.receive(packet))break;pending=true;offset=0;}
    if(!PakarS3::SshPolicy::admitted(packet.token,deviceSsh.active(),packet.revision,revision)){pending=false;continue;}
    if(packet.kind==PakarS3::SshTransport::Open){
      sshShell.reset(new(std::nothrow) DeviceShellSession);sshShellToken=packet.token;
      if(!sshShell){deviceSsh.close(packet.token);pending=false;continue;}
      initializeDeviceShell(*sshShell,false,true);sshShell->parser.size(packet.width,packet.height);
#if PAKAR_SCP_ENABLED
    }else if(packet.kind==PakarS3::SshTransport::FileRead){
      sshShell.reset();sshShellToken=0;sshScp=SshScpSession{};
      if(!packet.size||packet.size>64){deviceSsh.close(packet.token);pending=false;continue;}
      sshScp.token=packet.token;sshScp.revision=packet.revision;memcpy(sshScp.name,packet.bytes,packet.size);
    }else if(sshScp.token==packet.token&&packet.kind==PakarS3::SshTransport::Input){
      if(!sshScp.ready&&!sshScp.failed)break; // Retain initial ACK until metadata is checked.
      if(sshScp.ready&&!sshScp.finished)for(unsigned i=0;i<packet.size;++i)sshScp.download.input(packet.bytes[i]);
#endif
    }else if(sshShell&&sshShellToken==packet.token){
      if(packet.kind==PakarS3::SshTransport::Resize){sshShell->parser.size(packet.width,packet.height);if(sshShell->tui.active())shellTuiDraw(*sshShell,true);}
      else if(packet.kind==PakarS3::SshTransport::Input){
        // Retain the rest of a packet across loop turns, without replay. Match
        // Telnet's byte budget instead of draining a whole queue of commands.
        if(sshShell->output.length()>1024U)break;
        while(offset<packet.size&&inputBudget&&!sshShell->closing&&!sshShell->fault){inputDeviceShell(*sshShell,packet.bytes[offset++]);--inputBudget;terminalInput=true;}
        if(offset<packet.size&&!sshShell->closing&&!sshShell->fault)break;
      }
    }
    pending=false;
  }
#if PAKAR_SCP_ENABLED
  serviceSshScp();
#endif
  if(sshShell){auto&s=*sshShell;
    if(terminalInput&&s.tui.active()&&s.output.length()<4096U)shellTuiDraw(s);
    if(s.tui.active()&&s.tui.escapePending()&&uint32_t(millis()-s.escapeAt)>60)shellTuiInput(s,0,true);
    if(s.fault||uint32_t(millis()-s.lastInput)>1800000U||(s.output.length()&&uint32_t(millis()-s.lastDrain)>30000U))s.closing=true;
    const size_t count=min<size_t>(PakarS3::SshPolicy::packetBytes,s.output.length());
    if(count&&deviceSsh.output(sshShellToken,s.accessRevision,reinterpret_cast<const uint8_t*>(s.output.c_str()),count)){s.output.remove(0,count);s.lastDrain=millis();}
    if(s.closing&&!s.output.length())deviceSsh.finish(sshShellToken);
  }
  const uint16_t port=deviceSsh.listening()?deviceSsh.port():0;
  if(port!=sshAnnouncedPort){sshAnnouncedPort=port;startDiscovery();}
}
void registerSshApi(){
  server.on("/api/ssh",HTTP_GET,[](){if(authenticate())server.send(200,"application/json",sshStatusJson());});
  server.on("/api/ssh",HTTP_POST,[](){
    if(!authenticate())return;
    JsonDocument patch;
    if(server.hasArg("enabled")){const String value=server.arg("enabled");if(value!="true"&&value!="false"){server.send(400,"text/plain","Use true or false");return;}patch["enabled"]=value=="true";}
    if(server.hasArg("port")){uint32_t number=0;if(!PakarShell::unsignedNumber(server.arg("port").c_str(),65535,number)||!number){server.send(400,"text/plain","Invalid SSH port");return;}patch["port"]=number;}
    String result;const bool ok=updateSshConfiguration(patch.as<JsonObjectConst>(),result);
    JsonDocument reply;reply["accepted"]=ok;reply["message"]=result;JsonDocument status;deserializeJson(status,sshStatusJson());reply["ssh"]=status.as<JsonVariantConst>();
    String body;serializeJson(reply,body);server.send(ok?200:409,"application/json",body);
  });
}
#endif
uint16_t sshAdvertisedPort(){
#if PAKAR_SSH_ENABLED
  return deviceSsh.listening()?deviceSsh.port():0;
#else
  return 0;
#endif
}
