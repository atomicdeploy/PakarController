#include "../firmware/esp8266/src/RemoteFirmware.h"
#include <Updater.h>
#include <cassert>
#include <stdexcept>
#include <cstdio>
uint32_t testNow=1000;
fs::FS LittleFS;TestEsp ESP;TestUpdater Update;
std::map<std::string,TestResponse>testResponses;
std::vector<TestRequest>testRequests;
BearSSL::X509List &pakarTrustAnchor(){static BearSSL::X509List anchor;return anchor;}
unsigned prepares=0,programs=0,releases=0;
bool prepareOk=true,programOk=true,releaseOk=true,crashRelease=false,corruptEsp=false;
bool prepare(const RemoteFirmware::Plan&,String &why){++prepares;if(!prepareOk)why="prepare refused";return prepareOk;}
bool program(const char*,const String&,String &why){++programs;if(corruptEsp)LittleFS.files.at("/remote/esp.bin")->bytes[50]^=1;if(!programOk)why="program refused";return programOk;}
int8_t finish(const RemoteFirmware::Plan&,bool commit,String &why){
 if(!commit)return 1;
 ++releases;if(crashRelease)throw std::runtime_error("simulated power cut at release");
 if(!releaseOk){why="release ACK lost";return -1;}return 1;
}
String hash(const std::vector<uint8_t>&bytes){br_sha256_context c;br_sha256_init(&c);br_sha256_update(&c,bytes.data(),bytes.size());uint8_t out[32];char hex[65];br_sha256_out(&c,out);for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",out[i]);return hex;}
std::vector<uint8_t>bytes(const String&s){return {s.begin(),s.end()};}
struct Fixture{
 String url="https://updates.example.test/PakarController/manifest.json",sha;
 std::vector<uint8_t>esp=std::vector<uint8_t>(4096,0),avr=std::vector<uint8_t>(32768,0xff);
 Fixture(){
  testNow=1000;LittleFS=fs::FS{};ESP=TestEsp{};Update=TestUpdater{};testResponses.clear();testRequests.clear();
  prepares=programs=releases=0;prepareOk=programOk=releaseOk=true;crashRelease=corruptEsp=false;
  // Synthetic valid policy headers, NOT an executable board firmware image.
  esp[0]=0xe9;esp[1]=1;esp[2]=2;avr[0]=0x0c;avr[1]=0x94;avr.back()=1;
  JsonDocument d;d["product"]="PakarController";d["profile"]="pakar-nodemcu-atmega328p";d["link_mode"]="busy-wire";
  for(const char*key:{"esp","avr"}){bool e=!strcmp(key,"esp");auto o=d[key].to<JsonObject>();o["url"]=e?"https://updates.example.test/PakarController/esp.bin":"https://images.example.test/avr.bin";o["size"]=e?esp.size():avr.size();o["sha256"]=hash(e?esp:avr);o["version"]="0.2.8";o["model"]=e?"nodemcuv2":"atmega328p";if(!e){o["signature"]="1E950F";o["bootloader_included"]=true;}}
  String manifest;serializeJson(d,manifest);sha=hash(bytes(manifest));
  testResponses[url].bytes=bytes(manifest);testResponses["https://updates.example.test/PakarController/esp.bin"].bytes=esp;testResponses["https://images.example.test/avr.bin"].bytes=avr;
 }
 RemoteFirmware start(){RemoteFirmware j;j.begin("boot-A",prepare,program,finish);return j;}
 void stage(RemoteFirmware &j){assert(j.queue(url,sha,3,false,false,"private-test-bearer"));pump(j,"ready");assert(prepares==0&&programs==0&&releases==0);}
 static void pump(RemoteFirmware&j,const char*phase){for(unsigned i=0;i<300 && strcmp(j.phase(),phase) && j.busy();++i){testNow+=10;j.service();}assert(!strcmp(j.phase(),phase));}
 static JsonDocument state(const RemoteFirmware &j){JsonDocument d;assert(!deserializeJson(d,j.json()));return d;}
 static void apply(RemoteFirmware&j){auto d=state(j);assert(j.apply(d["job_id"],d["boot_id"].as<String>(),d["manifest_sha256"].as<String>()));}
};
int main(){
 {Fixture f;auto j=f.start();f.stage(j);assert(testRequests.size()==3);assert(testRequests[0].authorization=="Bearer private-test-bearer"&&testRequests[1].authorization=="Bearer private-test-bearer"&&testRequests[2].authorization.empty());assert(j.json().find("private-test-bearer")==String::npos);Fixture::apply(j);Fixture::pump(j,"awaiting_reboot");assert(prepares==1&&programs==1&&releases==0&&Update.commits==1);testNow+=2001;j.service();assert(ESP.restarts==1);RemoteFirmware after;after.begin("boot-B",prepare,program,finish);assert(!strcmp(after.phase(),"verify_pair"));after.service();assert(!strcmp(after.phase(),"complete")&&releases==1);}
 {Fixture f;auto j=f.start();f.stage(j);RemoteFirmware after;after.begin("boot-B",prepare,program,finish);assert(!strcmp(after.phase(),"ready"));assert(!after.apply(1,"boot-A",f.sha));assert(prepares==0);}
 {Fixture f;auto j=f.start();f.stage(j);LittleFS.files.at("/remote/esp.bin")->bytes[20]^=1;Fixture::apply(j);j.service();assert(!strcmp(j.phase(),"failed")&&prepares==0&&programs==0&&Update.commits==0);}
 {Fixture f;auto j=f.start();f.stage(j);Fixture::apply(j);RemoteFirmware after;after.begin("boot-B",prepare,program,finish);assert(!strcmp(after.phase(),"failed")&&prepares==0&&programs==0);}
 {Fixture f;auto j=f.start();f.stage(j);Fixture::apply(j);j.service();assert(!strcmp(j.phase(),"program_avr"));RemoteFirmware after;after.begin("boot-B",prepare,program,finish);assert(!strcmp(after.phase(),"failed")&&prepares==1&&programs==0&&releases==0);}
 {Fixture f;auto j=f.start();f.stage(j);prepareOk=false;Fixture::apply(j);j.service();assert(!strcmp(j.phase(),"failed")&&programs==0&&Update.commits==0);}
 {Fixture f;auto j=f.start();f.stage(j);programOk=false;Fixture::apply(j);Fixture::pump(j,"failed");assert(programs==1&&Update.commits==0&&releases==0);}
 {Fixture f;auto j=f.start();f.stage(j);corruptEsp=true;Fixture::apply(j);Fixture::pump(j,"failed");assert(programs==1&&Update.commits==0&&releases==0);}
 {Fixture f;auto j=f.start();f.stage(j);Update.failCommit=true;Fixture::apply(j);Fixture::pump(j,"failed");assert(programs==1&&Update.commits==0&&releases==0);}
 {Fixture f;auto j=f.start();f.stage(j);Fixture::apply(j);Fixture::pump(j,"awaiting_reboot");ESP.flash[10]^=1;RemoteFirmware after;after.begin("boot-B",prepare,program,finish);assert(!strcmp(after.phase(),"failed")&&releases==0);}
 {Fixture f;auto j=f.start();f.stage(j);Fixture::apply(j);Fixture::pump(j,"awaiting_reboot");releaseOk=false;RemoteFirmware after;after.begin("boot-B",prepare,program,finish);after.service();assert(!strcmp(after.phase(),"failed")&&releases==1);RemoteFirmware again;again.begin("boot-C",prepare,program,finish);again.service();assert(!strcmp(again.phase(),"failed")&&releases==1);}
 {Fixture f;auto j=f.start();f.stage(j);Fixture::apply(j);Fixture::pump(j,"awaiting_reboot");crashRelease=true;RemoteFirmware after;after.begin("boot-B",prepare,program,finish);try{after.service();assert(false);}catch(const std::runtime_error&){}assert(!strcmp(after.phase(),"release_pair")&&releases==1);RemoteFirmware again;again.begin("boot-C",prepare,program,finish);assert(!strcmp(again.phase(),"failed")&&releases==1);}
 {Fixture f;auto j=f.start();f.stage(j);LittleFS.failRename=true;auto d=Fixture::state(j);assert(!j.apply(d["job_id"],"boot-A",f.sha));assert(prepares==0&&programs==0);}
 {Fixture f;auto j=f.start();assert(j.queue(f.url,f.sha,3,false,false,""));assert(!j.cancel(2,"boot-A"));assert(!j.cancel(1,"boot-B"));assert(j.cancel(1,"boot-A"));assert(!strcmp(j.phase(),"cancelled")&&prepares==0);}
 for(unsigned which=0;which<4;++which){Fixture f;auto j=f.start();auto&r=testResponses[f.url];if(which==0)r.status=302;if(which==1)r.encoding="gzip";if(which==2)r.transfer="chunked";if(which==3)r.bytes[30]^=1;assert(j.queue(f.url,f.sha,3,false,false,""));Fixture::pump(j,"failed");assert(prepares==0&&programs==0&&releases==0);}
 {Fixture f;auto j=f.start();testResponses["https://updates.example.test/PakarController/esp.bin"].length=123;assert(j.queue(f.url,f.sha,3,false,false,""));Fixture::pump(j,"failed");assert(prepares==0&&programs==0);}
 {Fixture f;auto j=f.start();LittleFS.failWrite=true;assert(!j.queue(f.url,f.sha,3,false,false,""));assert(prepares==0&&programs==0);}
 {Fixture f;RemoteFirmware j;j.begin("",prepare,program,finish);assert(!j.queue(f.url,f.sha,3,false,false,""));assert(!strcmp(j.phase(),"failed"));}
 puts("Actual RemoteFirmware state machine: complete pair, staging-only, boot ownership, tampered stage/postboot, interrupted prepare/program/release journals, lost release ACK, callback/OTA/storage/HTTP failures, cross-origin bearer isolation and cancellation passed (host doubles, not physical/TLS proof).");
}
