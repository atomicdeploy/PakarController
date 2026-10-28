#include "ShellRemoteQuery.h"
#include <cassert>
#include <string>
#include <cstdio>
struct Socket {
  std::string input,output;unsigned binds=0,stops=0,flushes=0,replies=0;bool bindOk=true;
  bool begin(uint16_t port){assert(port==9876);++binds;return bindOk;}
  void stop(){++stops;}
  int parsePacket(){return input.size();}
  void flush(){input.clear();++flushes;}
  int read(uint8_t* bytes,int size){input.copy(reinterpret_cast<char*>(bytes),size);input.clear();return size;}
  int remoteIP(){return 17;}int remotePort(){return 2468;}
  bool beginPacket(int ip,int port){assert(ip==17&&port==2468);output.clear();return true;}
  void write(const uint8_t* data,size_t size){output.append(reinterpret_cast<const char*>(data),size);}
  void endPacket(){++replies;}
};
int main(){
  Socket s;PakarShell::RemoteQuery service;unsigned reads=0;
  auto status=[&](char* value,size_t capacity){++reads;snprintf(value,capacity,"hostname=fixture boot_id=boot uptime=1");};
  auto run=[&](uint32_t now,uint32_t epoch=1,bool enabled=true){service.service(s,now,epoch,enabled,status);};
  run(0,0);assert(s.binds==0);s.bindOk=false;run(1);run(100);assert(s.binds==1);
  s.bindOk=true;run(5001);assert(s.binds==2&&service.listening());
  s.input="0123456789abcdef status";run(5002);assert(s.replies==1&&reads==1&&s.output.find("0123456789abcdef OK hostname=fixture")==0);
  s.input="0123456789abcdef reset";run(5003);assert(s.replies==1&&s.flushes==1);
  s.input="fedcba9876543210 reset";run(5252);assert(s.replies==2&&reads==1&&s.output.find("fedcba9876543210 ERR read-only:")==0);
  s.input=std::string(65,'x');run(5502);assert(s.replies==2&&s.flushes==2);
  s.input="0123456789abcdef status\nreset";run(5502);assert(s.replies==2);
  s.input="0123456789abcdeg status";run(5502);assert(s.replies==2);
  s.input=std::string("0123456789abcdef status\0reset",28);run(5502);assert(s.replies==2);
  run(5503,2);assert(s.binds==3&&s.stops==1);run(5504,2,false);assert(!service.listening()&&s.stops==2);
  s.input="0123456789abcdef hello";run(0xffffff00U,2);assert(s.replies==3);
  s.input="0123456789abcdef status";run(10,2);assert(s.replies==4); // unsigned rollover
  puts("UDP bounded read-only query: lifecycle, bind backoff, correlation, rate limit and rejection pass.");
}
