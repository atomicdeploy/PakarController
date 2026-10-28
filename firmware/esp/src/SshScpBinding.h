#pragma once
#if PAKAR_SCP_ENABLED
// Included beside the shell on the application loop. Crypto task never touches
// LittleFS, the public index, flash scheduling or JSON ownership.
struct SshScpSession {
  uint32_t token=0,revision=0,listOffset=0;
  char name[65]{};String hash;
  bool ready=false,failed=false,finished=false;
  PakarS3::ScpDownload download;
} sshScp;
void serviceSshScp(){
  auto&s=sshScp;
  if(!s.token)return;
  if(!PakarS3::SshPolicy::admitted(s.token,deviceSsh.active(),s.revision,accessConfig.securityRevision())){s=SshScpSession{};return;}
  if(s.finished)return;
  auto emit=[&](const uint8_t* bytes,size_t count){return deviceSsh.output(s.token,s.revision,bytes,count);};
  if(s.failed){
    static const uint8_t message[]={1,'F','i','l','e',' ','u','n','a','v','a','i','l','a','b','l','e','\n'};
    if(emit(message,sizeof(message))){deviceSsh.finish(s.token,1);s.finished=true;}return;
  }
  if(!s.ready){
    JsonDocument request,response;request["action"]="list";request["offset"]=s.listOffset;
    if(deserializeJson(response,userFileRequest(request.as<JsonObjectConst>()))||response["accepted"]!=true){s.failed=true;return;}
    for(auto row:response["files"].as<JsonArrayConst>())if(row["name"].as<String>()==s.name){
      if(!UserFileStore<FirmwarePlatform::Sha256>::nameAllowed(s.name)||!row["size"].is<uint32_t>()||!UserFileStore<FirmwarePlatform::Sha256>::hashAllowed(row["sha256"]|"")){s.failed=true;return;}
      s.hash=row["sha256"].as<String>();s.ready=s.download.start(s.name,row["size"]);s.failed=!s.ready;return;
    }
    const uint32_t next=response["next_offset"]|0U;
    if(response["more"]==true&&next>s.listOffset&&next<32)s.listOffset=next;else s.failed=true;
    return;
  }
  s.download.service([&](uint32_t offset,uint8_t* bytes,size_t count){
    JsonDocument request,response;request["action"]="read";request["name"]=s.name;request["sha256"]=s.hash;
    request["offset"]=offset;request["count"]=uint16_t(count);
    if(deserializeJson(response,userFileRequest(request.as<JsonObjectConst>()))||response["accepted"]!=true)return false;
    const String hex=response["hex"]|"";if(hex.length()!=count*2)return false;
    for(size_t i=0;i<count;++i){
      const int hi=hex[i*2]>='0'&&hex[i*2]<='9'?hex[i*2]-'0':hex[i*2]>='a'&&hex[i*2]<='f'?hex[i*2]-'a'+10:-1;
      const int lo=hex[i*2+1]>='0'&&hex[i*2+1]<='9'?hex[i*2+1]-'0':hex[i*2+1]>='a'&&hex[i*2+1]<='f'?hex[i*2+1]-'a'+10:-1;
      if(hi<0||lo<0)return false;bytes[i]=uint8_t(hi*16+lo);
    }
    return true;
  },emit);
  if(s.download.phase()==PakarS3::ScpDownload::Failed){deviceSsh.close(s.token);s.finished=true;}
  else if(s.download.phase()==PakarS3::ScpDownload::Done){deviceSsh.finish(s.token);s.finished=true;}
}
#endif
