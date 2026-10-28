#include "../firmware/esp32s3/src/ScpDownload.h"
#include <cassert>
#include <string>
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(int argc,char** argv){
  // OpenSSH -S fixture: production SCP source over stdio, not an SSH server.
  // This independently checks real client interoperability, not encryption.
  if(argc>1){
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    char name[65];if(!PakarS3::ScpDownload::command(argv[argc-1],name,sizeof(name)))return 1;
    const uint32_t size=std::string(name)=="empty.bin"?0:513;
    PakarS3::ScpDownload transfer;if(!transfer.start(name,size))return 1;
    while(transfer.phase()!=PakarS3::ScpDownload::Done&&transfer.phase()!=PakarS3::ScpDownload::Failed){
      const auto phase=transfer.phase();
      if(phase==PakarS3::ScpDownload::ReadyAck||phase==PakarS3::ScpDownload::HeaderAck||phase==PakarS3::ScpDownload::FinalAck){
        char byte;if(!std::cin.get(byte))return 1;transfer.input(uint8_t(byte));
      }else transfer.service([](uint32_t offset,uint8_t* bytes,size_t count){for(size_t i=0;i<count;++i)bytes[i]=uint8_t(offset+i);return true;},
        [](const uint8_t* bytes,size_t count){std::cout.write(reinterpret_cast<const char*>(bytes),count);std::cout.flush();return bool(std::cout);});
    }
    return transfer.phase()==PakarS3::ScpDownload::Done?0:1;
  }
  using PakarS3::ScpDownload;char name[65];
  assert(ScpDownload::command("scp -f /user/example.bin",name,sizeof(name))&&std::string(name)=="example.bin");
  for(const char* text:{"scp -t /user/a","scp -r -f /user/a","scp -f /settings.json","scp -f /user/../a","scp -f /user/.secret","scp -f /user/a;reset","scp -f /user/a\n","scp -f /user/a b","scp -f /user/a/b","scp -f /user/a*"})assert(!ScpDownload::command(text,name,sizeof(name)));
  ScpDownload download;assert(download.start("test.bin",300));
  std::string wire;bool blocked=false;unsigned reads=0;
  auto emit=[&](const uint8_t* bytes,size_t count){if(blocked)return false;wire.append(reinterpret_cast<const char*>(bytes),count);return true;};
  auto read=[&](uint32_t offset,uint8_t* bytes,size_t count){++reads;for(size_t i=0;i<count;++i)bytes[i]=uint8_t(offset+i);return true;};
  download.service(read,emit);assert(wire.empty()&&!reads); // No data before client ACK.
  download.input(0);blocked=true;download.service(read,emit);assert(wire.empty());
  blocked=false;download.service(read,emit);const std::string header="C0644 300 test.bin\n";assert(wire==header);
  download.input(0);blocked=true;download.service(read,emit);assert(download.sent()==0);
  blocked=false;while(download.phase()==ScpDownload::Data)download.service(read,emit);
  assert(wire.size()==header.size()+301&&download.sent()==300);
  for(unsigned i=0;i<300;++i)assert(uint8_t(wire[header.size()+i])==uint8_t(i));
  assert(wire.back()==0&&download.phase()==ScpDownload::FinalAck);download.input(0);assert(download.phase()==ScpDownload::Done);
  ScpDownload empty;assert(empty.start("empty",0));empty.input(0);empty.service(read,emit);empty.input(0);empty.service(read,emit);assert(empty.phase()==ScpDownload::FinalAck);empty.input(0);assert(empty.phase()==ScpDownload::Done);
  ScpDownload rejected;assert(!rejected.start("../private",1));assert(!rejected.start("large",128*1024+1));
  ScpDownload fault;assert(fault.start("a",1));fault.input(0);fault.service(read,emit);fault.input(0);
  const auto before=wire.size();fault.service([](uint32_t,uint8_t*,size_t){return false;},emit);assert(fault.phase()==ScpDownload::Failed&&wire.size()==before);
  ScpDownload early;assert(early.start("a",1));early.input(0);early.input(0);assert(early.phase()==ScpDownload::Failed);
  std::puts("SCP source: exact binary/empty files, ACK ordering, queue backpressure, private/path/injection rejection and read-failure abort passed; no SSH wire/hardware proof.");
}
