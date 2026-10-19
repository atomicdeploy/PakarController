#include "../firmware/esp32s3/src/EthernetLldp.h"
#include <Arduino.h>
#include <freertos/task.h>
#include <cassert>
#include <cstdio>
#include <vector>
#include <future>
#include <thread>
#include <chrono>
namespace {
using namespace PakarS3;
const uint8_t mac[]={2,0,0,0,0,7};
std::vector<uint8_t> packet(uint16_t ttl=120) {
  std::vector<uint8_t> out(192);
  out.resize(Lldp::frame(out.data(),out.size(),mac,"fixture-switch",ttl));assert(!out.empty());return out;
}
void pure() {
  auto bytes=packet();Lldp::Neighbor peer;
  assert(Lldp::parse(bytes.data(),bytes.size(),123,peer));
  assert(peer.ttl==120&&peer.observedAt==123&&peer.chassisSubtype==4&&peer.portSubtype==5);
  assert(!strcmp(peer.chassis,"02:00:00:00:00:07")&&!strcmp(peer.port,"w5500")&&!strcmp(peer.systemName,"fixture-switch"));
  assert(Lldp::parse(bytes.data(),bytes.size()-2,123,peer)); // Modern optional End.
  assert(!Lldp::parse(bytes.data(),bytes.size()-1,123,peer));
  auto malformed=bytes;malformed[14]=4;assert(!Lldp::parse(malformed.data(),malformed.size(),123,peer));
  malformed=bytes;malformed[15]=0xff;assert(!Lldp::parse(malformed.data(),malformed.size(),123,peer));
  malformed=bytes;malformed[0]=0xff;assert(!Lldp::parse(malformed.data(),malformed.size(),123,peer));
  malformed=bytes;malformed[6]|=1;assert(!Lldp::parse(malformed.data(),malformed.size(),123,peer));
  // VLAN tag and unknown TLV are still bounded and preserve the mandatory order.
  auto tagged=bytes;tagged.insert(tagged.begin()+12,{0x81,0,0,7});
  assert(Lldp::parse(tagged.data(),tagged.size(),123,peer));
  auto optional=bytes;optional.insert(optional.end()-2,{0x1e,1,0xff});
  assert(Lldp::parse(optional.data(),optional.size(),123,peer));
  auto duplicate=bytes;duplicate.insert(duplicate.end()-2,{6,2,0,1});
  assert(!Lldp::parse(duplicate.data(),duplicate.size(),123,peer));
  // No terminal controls may escape a hostile name into a CLI/TUI consumer.
  auto hostile=bytes;
  for(size_t i=0;i+14<hostile.size();++i)if(!memcmp(hostile.data()+i,"fixture-switch",14)){hostile[i]=27;break;}
  assert(Lldp::parse(hostile.data(),hostile.size(),123,peer)&&peer.systemName[0]=='?');
  Lldp::Neighbors table;Lldp::Neighbor entries[Lldp::maxNeighbors];
  assert(Lldp::parse(bytes.data(),bytes.size(),0xfffffff0U,peer));peer.ttl=1;
  table.observe(peer,peer.observedAt);assert(table.copy(entries,0x20U)==1);
  assert(table.copy(entries,0x500U)==0); // Unsigned monotonic TTL rollover.
  for(unsigned i=0;i<6;++i){peer.key=i;peer.mac[5]=i;peer.observedAt=100+i;peer.ttl=120;table.observe(peer,100+i);}
  assert(table.copy(entries,110)==4);
  peer.key=5;peer.mac[5]=5;peer.ttl=0;table.observe(peer,111);assert(table.copy(entries,111)==3);
  table.clear();assert(!table.copy(entries,111));
  uint8_t small[160]{};assert(!Lldp::frame(small,20,mac,"ok"));
  assert(!Lldp::frame(small,sizeof(small),mac,"bad\nname"));
  auto shutdown=packet(0);assert(shutdown[35]==0&&shutdown[36]==0); // No optional TLV before End.
}
void attach(const void *owner,void *handle,bool registerInput=true) {
  EthernetLldpSetup scope(owner);
  if(registerInput)assert(esp_eth_update_input_path_info(handle,ethernetFixtureInput,handle)==ESP_OK);
  attachEthernetLldp(owner,handle,"ledboard",scope.admitted(handle));
}
void retire(const void *owner){detachEthernetLldp(owner);releaseEthernetLldp(owner);}
void observe(void *handle,const std::vector<uint8_t> &bytes) {
  auto input=ethernetSdk.inputs.at(handle);int info=123;
  auto *buffer=const_cast<uint8_t*>(bytes.data());
  assert(input.callback(handle,buffer,bytes.size(),input.priv,&info)==77);
  assert(ethernetSdk.forwardBuffer==buffer&&ethernetSdk.forwardLength==bytes.size());
  assert(ethernetSdk.forwardPriv==handle&&ethernetSdk.forwardInfo==&info);
}
void runtime() {
  int owner=0,other=0;void *handle=&owner;LldpStatus status;
  ethernetSdk=EthernetSdkFixture{};
  assert(esp_eth_update_input_path_info(&other,ethernetFixtureInput,&other)==ESP_OK);
  assert(ethernetSdk.inputs.at(&other).callback==ethernetFixtureInput); // Unrelated registrations unchanged.
  attach(&owner,handle);snapshotEthernetLldp(&owner,status,1000);
  assert(status.receiveSupported&&!status.receiveActive&&!status.promiscuous);
  assert(ethernetSdk.inputs.at(handle).callback!=ethernetFixtureInput);
  pakarTestMillis=1000;serviceEthernetLldp(&owner,true,mac,1000);
  assert(ethernetSdk.txCalls==0&&ethernetSdk.pending);
  ethernetSdk.onTransmit=[&] {
    std::promise<void> finished;auto ready=finished.get_future();
    std::thread loop([&]{serviceEthernetLldp(&owner,true,mac,1000);finished.set_value();});
    const bool skipped=ready.wait_for(std::chrono::milliseconds(250))==std::future_status::ready;
    if(!skipped)loop.detach();
    assert(skipped);loop.join();
  };
  ethernetSdk.drain();ethernetSdk.onTransmit={};
  assert(ethernetSdk.txCalls==1&&!ethernetSdk.promiscuous);
  auto bytes=packet();observe(handle,bytes);snapshotEthernetLldp(&owner,status,1000);assert(!status.received);
  assert(configureEthernetLldp(&owner,true,true));
  pakarTestMillis=2000;serviceEthernetLldp(&owner,true,mac,2000);assert(!ethernetSdk.promiscuous);
  ethernetSdk.drain();assert(ethernetSdk.promiscuous);
  pakarTestMillis=2100;observe(handle,bytes);snapshotEthernetLldp(&owner,status,2100);
  assert(status.receiveActive&&status.received==1&&status.neighborCount==1);
  observe(handle,bytes);snapshotEthernetLldp(&owner,status,2100);assert(status.received==1&&status.dropped>=1);
  pakarTestMillis=2300;auto broken=bytes;broken[15]=0xff;observe(handle,broken);
  snapshotEthernetLldp(&owner,status,2300);assert(status.invalid==1&&status.neighborCount==1);
  pakarTestMillis=2500;serviceEthernetLldp(&owner,false,mac,2500);
  pakarTestMillis=2600;observe(handle,bytes);snapshotEthernetLldp(&owner,status,2600);
  assert(!status.receiveActive&&!status.neighborCount&&status.received==1); // Late RX on a lost link is not a new neighbor.
  serviceEthernetLldp(&owner,true,mac,2600);
  assert(configureEthernetLldp(&owner,true,false));snapshotEthernetLldp(&owner,status,2700);
  assert(!status.receiveActive&&!status.neighborCount&&status.promiscuous); // Desired vs actual filter ACK differs.
  pakarTestMillis=3000;serviceEthernetLldp(&owner,true,mac,3000);ethernetSdk.drain();assert(!ethernetSdk.promiscuous);
  ethernetSdk.queueResult=false;pakarTestMillis=34000;serviceEthernetLldp(&owner,true,mac,34000);
  const auto attempts=ethernetSdk.queueCalls;serviceEthernetLldp(&owner,true,mac,34001);assert(ethernetSdk.queueCalls==attempts);
  ethernetSdk.queueResult=true;pakarTestMillis=35000;serviceEthernetLldp(&owner,true,mac,35000);
  assert(ethernetSdk.pending);const auto transmitted=ethernetSdk.txCalls;
  retire(&owner);attach(&other,&other);ethernetSdk.drain();assert(ethernetSdk.txCalls==transmitted); // Epoch revokes queued old-owner TX.
  pakarTestMillis=36000;serviceEthernetLldp(&other,true,mac,36000);ethernetSdk.drain();assert(ethernetSdk.txCalls==transmitted+1);
  assert(configureEthernetLldp(&other,false,false));pakarTestMillis=37000;
  serviceEthernetLldp(&other,true,mac,37000);ethernetSdk.drain();
  Lldp::Neighbor withdrawal;assert(Lldp::parse(ethernetSdk.transmitted.data(),ethernetSdk.transmitted.size(),37000,withdrawal)&&withdrawal.ttl==0);
  retire(&other);
  // An unwitnessed/missing wrap cannot silently activate promiscuous receive.
  attach(&owner,handle,false);assert(!configureEthernetLldp(&owner,true,true));
  snapshotEthernetLldp(&owner,status,36000);assert(!status.receiveSupported&&!status.receiveActive&&!status.promiscuous);
  retire(&owner);
  { EthernetLldpSetup scope(&owner);
    ethernetCurrentTask=reinterpret_cast<void*>(2);
    assert(esp_eth_update_input_path_info(&other,ethernetFixtureInput,&other)==ESP_OK);
    assert(ethernetSdk.inputs.at(&other).callback==ethernetFixtureInput&&!scope.admitted(&other));
    ethernetCurrentTask=reinterpret_cast<void*>(1);
    assert(esp_eth_update_input_path_info(handle,ethernetFixtureInput,handle)==ESP_OK&&scope.admitted(handle));
    EthernetLldpSetup nested(&other);assert(!nested.admitted(handle));
    assert(esp_eth_update_input_path_info(&other,ethernetFixtureInput,&other)==ESP_OK&&!scope.admitted(handle));
    retire(&owner);
  }
  { EthernetLldpSetup scope(&owner);ethernetSdk.inputResult=false;
    assert(esp_eth_update_input_path_info(handle,ethernetFixtureInput,handle)!=ESP_OK&&!scope.admitted(handle));
    ethernetSdk.inputResult=true;retire(&owner);
  }
}
}
int main(){pure();runtime();puts("Actual bounded LLDP codec/observer: unchanged ownership/priv/info, explicit receive/filter ACK, TCPIP-only TX, TTL/withdrawal/queue/epoch fences passed with SDK doubles, not hardware.");}
