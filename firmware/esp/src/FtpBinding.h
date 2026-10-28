#pragma once
#if PAKAR_FTP_ENABLED
#include "FileTransferBinding.h"
namespace DeviceFtp {
ManagedTransferFiles files(transferFileRequest);
using Session=PakarS3::FtpSession<ManagedTransferFiles>;
Session session(files);
Session::Action direction=Session::None;
WiFiServer listener,passive;
WiFiClient control,data;
struct Settings {uint32_t magic=0x504b4654;uint16_t port=21,passivePort=50009;uint8_t enabled=0,reserved[3]{};uint32_t check=0;} settings;
bool ready=false,listening=false,passiveOpen=false,transferring=false,closing=false;
uint32_t security=0,lastActivity=0,passiveAt=0,offset=0;unsigned failures=0,listOffset=0;
char line[256]{},out[2048]{};size_t lineSize=0,outSize=0,outAt=0;
uint8_t bytes[1536]{};size_t dataSize=0,dataAt=0;
uint32_t checksum(const Settings& s){uint32_t h=2166136261U;auto* p=reinterpret_cast<const uint8_t*>(&s);for(size_t i=0;i<offsetof(Settings,check);++i){h^=p[i];h*=16777619U;}return h;}
void stopTransfer(){files.cancel();data.stop();passive.stop();passiveOpen=transferring=false;direction=Session::None;dataSize=dataAt=0;}
void stop(){stopTransfer();control.stop();listener.stop();listening=false;closing=false;session.authenticated=false;lineSize=outSize=outAt=0;}
void reply(const char* text){const size_t n=strlen(text);if(n>sizeof(out)-outSize){closing=true;return;}memcpy(out+outSize,text,n);outSize+=n;}
void begin(){Preferences p;if(!p.begin("pakar-ftp",false))return;
  if(!p.isKey("settings")){ready=true;p.end();return;}
  Settings s;const bool ok=p.getBytesLength("settings")==sizeof(s)&&p.getBytes("settings",&s,sizeof(s))==sizeof(s)&&s.magic==settings.magic&&s.enabled<=1&&s.port&&s.passivePort>=49152&&s.port!=s.passivePort&&s.check==checksum(s);
  p.end();if(ok){settings=s;ready=true;}
}
bool configure(bool enabled,uint16_t port,uint16_t passivePort){
  if(!ready||!port||port==80||port==443||port==23||port==22||port==shellSettings.port()||passivePort<49152||port==passivePort)return false;
#if PAKAR_SSH_ENABLED
  if(port==deviceSsh.port()||passivePort==deviceSsh.port())return false;
#endif
  if(passivePort==shellSettings.port())return false;
  Settings s=settings;s.enabled=enabled;s.port=port;s.passivePort=passivePort;s.check=checksum(s);
  if(!memcmp(&s,&settings,sizeof(s)))return true;
  Preferences p;if(!p.begin("pakar-ftp",false))return false;Settings verify;
  const bool ok=p.putBytes("settings",&s,sizeof(s))==sizeof(s)&&p.getBytes("settings",&verify,sizeof(verify))==sizeof(verify)&&!memcmp(&s,&verify,sizeof(s));p.end();
  if(ok){stop();settings=s;}return ok;
}
String status(){JsonDocument d;d["built"]=true;d["settings_ready"]=ready;d["enabled"]=bool(settings.enabled);d["listening"]=listening;d["port"]=settings.port;d["passive_port"]=settings.passivePort;
  d["authentication_required"]=true;d["encrypted"]=false;d["namespace"]="/user/";d["max_file_bytes"]=ManagedTransferFiles::limit;d["active"]=transferring;d["bytes"]=offset;d["session_limit"]=1;String s;serializeJson(d,s);return s;}
bool flush(WiFiClient& peer,const void* buffer,size_t size,size_t& sent){
  const int n=send(peer.fd(),static_cast<const uint8_t*>(buffer)+sent,size-sent,MSG_DONTWAIT);
  if(n>0){sent+=n;lastActivity=millis();return true;}
  return n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR);
}
void command(){
  line[lineSize]=0;session.command(line,accessConfig.username(),accessConfig.password());lineSize=0;
  memset(line,0,sizeof(line)); // PASS bytes are not retained in the command buffer.
  reply(session.response());
  if(!strncmp(session.response(),"530 Authentication failed",25)&&++failures>=3)closing=true;
  switch(session.action){
    case Session::Quit:closing=true;break;
    case Session::Abort:stopTransfer();break;
    case Session::Passive:{
      stopTransfer();passive.begin(settings.passivePort);passiveOpen=bool(passive);passiveAt=millis();
      if(!passiveOpen){reply("425 Passive listener unavailable\r\n");break;}
      char response[100];if(session.extended)snprintf(response,sizeof(response),"229 Entering Extended Passive Mode (|||%u|)\r\n",settings.passivePort);
      else{const IPAddress ip=control.localIP();snprintf(response,sizeof(response),"227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n",ip[0],ip[1],ip[2],ip[3],settings.passivePort>>8,settings.passivePort&255);}reply(response);break;
    }
    case Session::Store:case Session::Retrieve:case Session::List:
      if(!passiveOpen){reply("425 Use PASV or EPSV first\r\n");break;}
      if(session.action!=Session::List&&!files.open(session.path,session.action==Session::Store)){stopTransfer();reply("550 File unavailable or maintenance busy\r\n");break;}
      direction=session.action;transferring=true;offset=listOffset=0;dataSize=dataAt=0;reply("150 Opening binary data connection\r\n");break;
    default:break;
  }
}
void service(){
  const uint32_t now=millis();
  const bool allowed=ready&&settings.enabled&&accessConfig.ready()&&(NetworkPath::ready()||apStarted)&&!shellBusy()&&!networkApplyPending&&!cloudApplyPending;
  if(!allowed||security!=accessConfig.securityRevision()){stop();security=accessConfig.securityRevision();if(!allowed)return;}
  if(!listening){listener.begin(settings.port);listening=bool(listener);if(!listening)return;}
  if(control.fd()<0){
    WiFiClient next=listener.accept();if(next){control=next;control.setNoDelay(true);session.authenticated=false;closing=false;failures=0;lineSize=outSize=outAt=0;lastActivity=now;reply("220 PakarController public file service\r\n");}return;
  }
  if(uint32_t(now-lastActivity)>60000U){stop();return;}
  if(outSize){if(!flush(control,out,outSize,outAt)){stop();return;}if(outAt==outSize)outSize=outAt=0;return;}
  if(closing){stop();return;}
  // Control remains responsive during data transfer. ABOR cancels staging;
  // NOOP cannot accidentally replace the latched upload/download direction.
  for(unsigned budget=0;budget<128;++budget){uint8_t byte;const int n=recv(control.fd(),&byte,1,MSG_DONTWAIT);
    if(n==0){stop();return;}if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){stop();return;}break;}
    lastActivity=now;if(byte=='\r')continue;
    if(byte=='\n'){
      if(transferring){line[lineSize]=0;for(size_t i=0;i<lineSize;++i)if(line[i]>='a'&&line[i]<='z')line[i]-=32;
        if(strcmp(line,"ABOR")&&strcmp(line,"QUIT")&&strcmp(line,"NOOP")){reply("450 Transfer in progress\r\n");lineSize=0;memset(line,0,sizeof(line));return;}}
      command();return;
    }
    if(byte<32||byte>126||lineSize==sizeof(line)-1){closing=true;reply("500 Invalid command\r\n");return;}line[lineSize++]=byte;
  }
  if(passiveOpen&&data.fd()<0){
    WiFiClient next=passive.accept();if(next){if(next.remoteIP()==control.remoteIP()){data=next;data.setNoDelay(true);}else next.stop();}
    if(uint32_t(now-passiveAt)>15000U){stopTransfer();reply("425 Data connection timed out\r\n");return;}
  }
  if(transferring&&data.fd()>=0){
    if(direction==Session::Store){
      const int n=recv(data.fd(),bytes,512,MSG_DONTWAIT);
      if(n>0){if(!files.write(offset,bytes,n)){stopTransfer();reply("552 Upload rejected\r\n");return;}offset+=n;lastActivity=now;}
      else if(n==0){const bool ok=files.close();stopTransfer();reply(ok?"226 Upload committed\r\n":"451 Commit failed\r\n");}
      else if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){stopTransfer();reply("426 Upload interrupted\r\n");}
    }else{
      if(dataSize){if(!flush(data,bytes,dataSize,dataAt)){stopTransfer();reply("426 Transfer interrupted\r\n");return;}if(dataAt==dataSize)dataSize=dataAt=0;return;}
      bool done=false;
      if(direction==Session::Retrieve){size_t count=512;if(!files.read(offset,bytes,count)){stopTransfer();reply("451 Read failed\r\n");return;}dataSize=count;offset+=count;done=!count;}
      else{ManagedTransferFiles::Info rows[8];unsigned count=0;if(!files.list(listOffset,rows,count)){stopTransfer();reply("451 Index unavailable\r\n");return;}listOffset+=count;done=!count;
        for(unsigned i=0;i<count;++i){const int n=session.namesOnly?snprintf(reinterpret_cast<char*>(bytes)+dataSize,sizeof(bytes)-dataSize,"%s\r\n",rows[i].name):snprintf(reinterpret_cast<char*>(bytes)+dataSize,sizeof(bytes)-dataSize,"-rw-r--r-- 1 user user %lu Jan 01 1970 %s\r\n",static_cast<unsigned long>(rows[i].size),rows[i].name);if(n<=0||size_t(n)>=sizeof(bytes)-dataSize){stopTransfer();reply("451 Listing too large\r\n");return;}dataSize+=n;}}
      if(done){stopTransfer();reply("226 Transfer complete\r\n");}
    }
    return;
  }
}
void api(){
  server.on("/api/ftp",HTTP_GET,[](){if(authenticate())server.send(200,"application/json",status());});
  server.on("/api/ftp",HTTP_POST,[](){
    if(!authenticate())return;if(!maintenanceApiIdle()){server.send(409,"text/plain","Maintenance active");return;}
    bool enabled=settings.enabled;uint32_t port=settings.port,passivePort=settings.passivePort;
    if(server.hasArg("enabled")){const String value=server.arg("enabled");if(value!="true"&&value!="false"){server.send(400,"text/plain","Use true or false");return;}enabled=value=="true";}
    if((server.hasArg("port")&&(!PakarShell::unsignedNumber(server.arg("port").c_str(),65535,port)||!port))||(server.hasArg("passive_port")&&(!PakarShell::unsignedNumber(server.arg("passive_port").c_str(),65535,passivePort)||!passivePort))){server.send(400,"text/plain","Invalid port");return;}
    if(!configure(enabled,port,passivePort)){server.send(409,"text/plain","Invalid/conflicting ports or durable save failed");return;}
    PakarConfiguration::changes().saved(PakarConfiguration::Shell);server.send(200,"application/json",status());
  });
}
}
#endif
