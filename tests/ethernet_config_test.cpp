#include "../firmware/esp8266/src/EthernetConfig.h"
#include <cassert>
#include <iostream>
namespace {
struct Stored {
  uint32_t magic;uint8_t layout,dhcp;uint8_t addresses[5][4];uint32_t checksum;
} __attribute__((packed));
void digest(Stored &record) {
  record.checksum=2166136261U;
  const auto *bytes=reinterpret_cast<const uint8_t*>(&record);
  for(size_t i=0;i<offsetof(Stored,checksum);++i)record.checksum=(record.checksum^bytes[i])*16777619U;
}
void assertOld(fs::FS &filesystem,const std::vector<uint8_t> &bytes) {
  assert(filesystem.files.at("/ethernet.bin")->bytes==bytes);
  EthernetConfigStore loaded;assert(loaded.begin(filesystem));
  assert(!loaded.dhcpEnabled()&&loaded.address().toString()=="192.0.2.7");
}
}
int main() {
  fs::FS filesystem;EthernetConfigStore active;
  assert(active.begin(filesystem)&&active.dhcpEnabled()&&filesystem.faults->writes==0);
  assert(active.setAddressing(false,"192.0.2.7","192.0.2.1","255.255.255.0","192.0.2.1",""));
  assert(active.save());const auto original=filesystem.files.at("/ethernet.bin")->bytes;
  const auto writes=filesystem.faults->writes,renames=filesystem.faults->renames;
  assert(active.save()&&filesystem.faults->writes==writes&&filesystem.faults->renames==renames);
  EthernetConfigStore candidate=active;
  assert(candidate.setAddressing(true));assert(candidate.dhcpEnabled()&&candidate.address().toString()=="192.0.2.7");
  bool* faults[]={&filesystem.faults->failOpenWrite,&filesystem.faults->failWrite,&filesystem.faults->shortWrite,
      &filesystem.faults->failVerifyRead,&filesystem.faults->corruptVerify,&filesystem.faults->failRename};
  for(bool* fault:faults) {*fault=true;assert(!candidate.save());assertOld(filesystem,original);*fault=false;}
  assert(!active.dhcpEnabled());assert(candidate.save());active=candidate;
  EthernetConfigStore restored;assert(restored.begin(filesystem)&&restored.dhcpEnabled());
  assert(restored.address().toString()=="192.0.2.7"&&restored.dns2().toString()=="0.0.0.0");
  assert(restored.setAddressing(false,restored.address().toString(),restored.gateway().toString(),
      restored.subnet().toString(),restored.dns1().toString(),restored.dns2().toString()));
  for(const char* bad:{"bad","192.0.2.0","192.0.2.255","192.0.2.1","224.0.0.1","0.0.0.1"})
    assert(!restored.setAddressing(false,bad,"192.0.2.1","255.255.255.0"));
  assert(!restored.setAddressing(false,String("192.0.2.7\0x",11),"192.0.2.1","255.255.255.0"));
  assert(!restored.setAddressing(false,"192.0.2.7","192.0.3.1","255.255.255.0"));
  assert(!restored.setAddressing(false,"192.0.2.7","192.0.2.1","255.0.255.0"));
  assert(restored.address().toString()=="192.0.2.7");
  for(unsigned fault=0;fault<6;++fault) {
    Stored damaged{};memcpy(&damaged,original.data(),sizeof(damaged));
    switch(fault) {
      case 0:damaged.magic=0;break;case 1:damaged.layout=2;break;case 2:damaged.dhcp=2;break;
      case 3:damaged.addresses[2][1]=0;break;case 4:damaged.addresses[3][0]=224;break;
      case 5:damaged.dhcp=1;damaged.addresses[0][0]=0;break;
    }
    digest(damaged);auto data=std::make_shared<File::Data>();
    const auto *bytes=reinterpret_cast<const uint8_t*>(&damaged);data->bytes.assign(bytes,bytes+sizeof(damaged));
    fs::FS bad;bad.files["/ethernet.bin"]=data;EthernetConfigStore rejected;
    assert(!rejected.begin(bad)&&rejected.dhcpEnabled()&&String(rejected.lastError()).length());
    assert(bad.faults->writes==0&&bad.files.at("/ethernet.bin")->bytes==data->bytes);
  }
  fs::FS orphan;orphan.files["/ethernet.pending"]=filesystem.files.at("/ethernet.bin");
  EthernetConfigStore fresh;assert(fresh.begin(orphan)&&fresh.dhcpEnabled()&&orphan.faults->writes==0);
  std::cout<<"Independent Ethernet profile: DHCP defaults, IPv4 validation, saved static preservation, atomic fault/no-op/candidate publish and corrupt record rejection passed\n";
}
