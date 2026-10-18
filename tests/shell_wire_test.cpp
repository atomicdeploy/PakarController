#include "../firmware/esp8266/src/ShellWire.h"
#include "../firmware/esp8266/src/ShellSettings.h"
#include <cassert>
#include <cstdio>
int main(){
  using C=PakarShell::SerialChannel;C channel;const uint8_t command[]={'e','c','h','o',' ','o','k','\r'};
  assert(channel.accept(1,0,command,sizeof(command))==C::Stale);channel.open(42);
  assert(channel.accept(41,0,command,sizeof(command))==C::Stale);
  assert(channel.accept(42,1,command,sizeof(command))==C::Sequence);
  assert(channel.accept(42,0,command,sizeof(command))==C::Accepted);
  assert(channel.accept(42,0,command,sizeof(command))==C::Duplicate);
  const uint8_t different[]={'r','e','s','e','t','\r'};
  assert(channel.accept(42,0,different,sizeof(different))==C::Sequence);
  for(uint32_t i=1;i<UINT16_MAX;++i)assert(channel.accept(42,uint16_t(i),command,sizeof(command))==C::Accepted);
  assert(channel.accept(42,UINT16_MAX,command,sizeof(command))==C::Sequence);
  assert(channel.accept(42,0,command,sizeof(command))==C::Sequence);
  channel.close();channel.open(43);assert(channel.accept(42,0,command,sizeof(command))==C::Stale);
  PakarShell::OutputWindow output;size_t consumed=99;
  assert(output.acknowledge(65535,consumed)&&consumed==0);output.stage(command,sizeof(command));
  assert(output.size()==sizeof(command)&&output.sequence()==0);
  assert(output.acknowledge(65535,consumed)&&consumed==0); // Lost read reply replays same bytes, never consumes them.
  assert(!output.acknowledge(7,consumed));
  assert(output.acknowledge(0,consumed)&&consumed==sizeof(command));
  assert(output.acknowledge(0,consumed)&&consumed==0&&output.sequence()==1);
  for(uint32_t i=1;i<UINT16_MAX;++i){assert(output.stage(command,sizeof(command)));assert(output.sequence()==i);assert(output.acknowledge(uint16_t(i),consumed));}
  assert(output.sequence()==UINT16_MAX&&!output.stage(command,sizeof(command)));
  assert(!output.acknowledge(0,consumed)&&!output.acknowledge(UINT16_MAX,consumed)&&consumed==0);
  uint32_t n=0;assert(PakarShell::unsignedNumber("4294967295",UINT32_MAX,n)&&n==UINT32_MAX);
  for(const char *bad:{"","-1","1,2","1.0","1x","4294967296","999999999999999999999"})assert(!PakarShell::unsignedNumber(bad,UINT32_MAX,n));
  fs::FS fs;ShellSettings settings;assert(settings.begin(fs)&&settings.port()==23&&settings.enabled());
  assert(settings.configure(true,2323,true));const auto original=fs.files["/shell.bin"];
  assert(settings.configure(true,2323,true)&&fs.files["/shell.bin"]==original); // No identical-write churn.
  assert(!settings.configure(true,80,true)&&settings.port()==2323);
  fs.failRename=true;assert(!settings.configure(false,24,false)&&settings.enabled()&&settings.port()==2323);fs.failRename=false;
  ShellSettings restored;assert(restored.begin(fs)&&restored.port()==2323);
  fs.files["/shell.bin"]->bytes[2]^=1;ShellSettings corrupt;assert(!corrupt.begin(fs)&&!corrupt.enabled());
  puts("Shell wire: boot-owner token seam, duplicate byte admission, no sequence wrap, strict integers; durable settings/readback/unchanged suppression/corruption failclosed passed.");
}
