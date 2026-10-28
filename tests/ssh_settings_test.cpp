#include "../firmware/esp32s3/src/SshTransport.h"
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
// Real SshTransport::begin/configure with an in-memory NVS boundary only.
static std::map<std::string,std::vector<uint8_t>> storage;
static bool openAllowed=true,writeAllowed=true,readAllowed=true;
static unsigned writes=0;
bool Preferences::begin(const char* name,bool){assert(std::string(name)=="pakar-ssh");return openAllowed;}
void Preferences::end(){}
bool Preferences::isKey(const char* key){return storage.count(key);}
size_t Preferences::getBytesLength(const char* key){return storage[key].size();}
size_t Preferences::getBytes(const char* key,void* out,size_t count){if(!readAllowed||storage[key].size()!=count)return 0;memcpy(out,storage[key].data(),count);return count;}
size_t Preferences::putBytes(const char* key,const void* input,size_t count){if(!writeAllowed)return 0;++writes;const auto* bytes=static_cast<const uint8_t*>(input);storage[key]={bytes,bytes+count};return count;}
int main(){
  using PakarS3::SshTransport;
  SshTransport fresh;assert(fresh.begin()&&!fresh.enabled()&&fresh.port()==22&&writes==0);
  for(auto port:{0,23,80,443,7777})assert(!fresh.configure(true,port,23));
  assert(fresh.configure(true,2222,23)&&fresh.enabled()&&fresh.port()==2222&&writes==1);
  assert(fresh.configure(true,2222,23)&&writes==1); // No identical writes.
  SshTransport reboot;assert(reboot.begin()&&reboot.enabled()&&reboot.port()==2222);
  writeAllowed=false;assert(!reboot.configure(false,2222,23)&&reboot.enabled());writeAllowed=true;
  assert(reboot.configure(false,2222,23)&&!reboot.enabled()&&writes==2);
  SshTransport disabled;assert(disabled.begin()&&!disabled.enabled());
  storage["settings"][0]^=1;
  SshTransport corrupt;assert(!corrupt.begin()&&!corrupt.enabled());assert(*corrupt.error());
  assert(!corrupt.configure(true,22,23)); // Never erase corruption to repair it.
  storage.clear();openAllowed=false;
  SshTransport unavailable;assert(!unavailable.begin()&&!unavailable.enabled());
  openAllowed=true;
  SshTransport readback;assert(readback.begin());readAllowed=false;
  assert(!readback.configure(true,22,23)&&!readback.enabled());
  std::puts("Actual SSH NVS settings: disabled default, reserved ports, persisted enable/port, identical-write suppression and failure/corruption gates passed; not flash/NVS hardware acceptance.");
}
