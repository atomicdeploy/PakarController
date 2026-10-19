#include "../firmware/esp32s3/src/NbnsService.h"
#include <cassert>
#include <cstdio>
using namespace PakarNbns;
static std::vector<uint8_t> query(const char* name,uint16_t type=0x20,uint8_t suffix=0){
  std::vector<uint8_t> result(kQuestionBytes);result[0]=0x12;result[1]=0x34;put16(result.data()+2,0x0110);put16(result.data()+4,1);result[12]=32;
  uint8_t raw[16]{};if(!strcmp(name,"*"))raw[0]='*';else{memset(raw,' ',15);memcpy(raw,name,strlen(name));}raw[15]=suffix;
  for(unsigned i=0;i<16;++i){result[13+i*2]=uint8_t('A'+(raw[i]>>4));result[14+i*2]=uint8_t('A'+(raw[i]&15));}
  put16(result.data()+46,type);put16(result.data()+48,1);return result;
}
static void reset(PakarS3::NbnsService& service){service.end();WiFi=NbnsFixtureWifi{};ETH=NbnsFixtureEthernet{};NbnsFixture::sockets.clear();NbnsFixture::sent.clear();NbnsFixture::failDevice.clear();NbnsFixture::failSend=false;pakarTestMillis=100;}
static void ethernet(){ETH.ready=true;ETH.address=IPAddress(192,168,1,20);}
int main(){
  char name[16];assert(hostname("ledboard",name)&&!strcmp(name,"LEDBOARD"));
  assert(hostname("a12345678901234",name));
  for(const char* invalid:{"","a123456789012345","-bad","bad-","bad.name","bad name"})assert(!hostname(invalid,name)&&!name[0]);
  assert(hostname("ledboard",name));
  auto q=query("LEDBOARD");uint8_t output[kMaximumReply]{};const uint8_t mac[6]={2,3,4,5,6,7};
  assert(answer(q.data(),q.size(),name,0xc0a80114,mac,output,sizeof(output))==62);
  assert(get16(output)==0x1234&&get16(output+2)==0x8500&&get16(output+6)==1&&get16(output+54)==6);
  assert(output[58]==192&&output[59]==168&&output[60]==1&&output[61]==20);
  auto status=query("*",0x21);assert(answer(status.data(),status.size(),name,0xc0a80114,mac,output,sizeof(output))==121);
  assert(get16(output+46)==0x21&&get16(output+54)==65&&output[56]==1&&get16(output+73)==0x0400);
  assert(!memcmp(output+57,"LEDBOARD       ",15)&&!memcmp(output+75,mac,6));
  for(size_t n=0;n<kQuestionBytes;++n)assert(!answer(q.data(),n,name,0xc0a80114,mac,output,sizeof(output)));
  for(unsigned at:{2U,4U,6U,8U,10U,12U,13U,45U,46U,48U}){auto bad=q;bad[at]=0xff;assert(!answer(bad.data(),bad.size(),name,0xc0a80114,mac,output,sizeof(output)));}
  auto smb=query("LEDBOARD",0x20,0x20);assert(!answer(smb.data(),smb.size(),name,0xc0a80114,mac,output,sizeof(output)));
  auto other=query("OTHER");assert(!answer(other.data(),other.size(),name,0xc0a80114,mac,output,sizeof(output)));
  assert(!answer(q.data(),q.size(),name,0xc0a80114,mac,output,61));

  PakarS3::NbnsService service;reset(service);assert(!service.begin("ledboard")&&!service.running());
  ethernet();service.service();assert(service.running()&&service.boundInterfaces()==1);
  assert(NbnsFixture::queue("en1",q,0xc0a80164,50000));service.service();
  assert(NbnsFixture::sent.size()==1&&NbnsFixture::sent.back().device=="en1"&&NbnsFixture::sent.back().port==50000);
  assert(NbnsFixture::sent.back().bytes[61]==20); // ETH-only answers its own address.
  WiFi.connection=WL_CONNECTED;WiFi.mode=WIFI_AP_STA;WiFi.address=IPAddress(192,168,1,21);WiFi.ap=IPAddress(192,168,4,1);service.service();
  assert(service.boundInterfaces()==3);
  for(const char* device:{"en1","st1","ap1"})assert(NbnsFixture::queue(device,status,!strcmp(device,"ap1")?0xc0a80464:0xc0a80164));
  service.service();assert(NbnsFixture::sent.size()==4);
  const auto& eth=NbnsFixture::sent[1];const auto& sta=NbnsFixture::sent[2];const auto& ap=NbnsFixture::sent[3];
  assert(eth.bytes[80]==3&&sta.bytes[80]==1&&ap.bytes[80]==2); // Same-subnet ETH/STA exact ingress MAC.
  pakarTestMillis+=100;assert(NbnsFixture::queue("en1",q,0xc0a80164)&&NbnsFixture::queue("st1",q,0xc0a80164));service.service();
  assert(NbnsFixture::sent[NbnsFixture::sent.size()-2].bytes[61]==20&&NbnsFixture::sent.back().bytes[61]==21);
  ETH.ready=false;service.service();assert(service.boundInterfaces()==2);
  WiFi.connection=0;WiFi.mode=WIFI_OFF;service.service();assert(!service.running());

  reset(service);ethernet();assert(service.begin("ledboard"));
  assert(NbnsFixture::queue("en1",q,0xc0a80164)&&NbnsFixture::queue("en1",q,0xc0a80164));service.service();
  assert(NbnsFixture::sent.size()==1&&NbnsFixture::sockets.back().input.size()==1); // One datagram/pass.
  service.service();assert(NbnsFixture::sent.size()==2);
  assert(NbnsFixture::queue("en1",q,0xc0a80164));service.service();assert(NbnsFixture::sent.size()==2); // Bounded two-query burst.
  pakarTestMillis+=100;assert(NbnsFixture::queue("en1",q,0xc0a80164));service.service();assert(NbnsFixture::sent.size()==3);
  pakarTestMillis+=100;NbnsFixture::failSend=true;assert(NbnsFixture::queue("en1",q,0xc0a80164));service.service();const auto failed=service.sendFailures();
  service.service();assert(failed&&service.sendFailures()==failed);NbnsFixture::failSend=false;
  assert(service.begin("renamed"));assert(NbnsFixture::queue("en1",q,0xc0a80164));service.service();const auto before=NbnsFixture::sent.size();
  assert(NbnsFixture::queue("en1",query("RENAMED"),0xc0a80164));service.service();assert(NbnsFixture::sent.size()==before+1);
  assert(!service.begin("sixteencharacters")&&!service.running()); // No truncated alias/listener.

  reset(service);ethernet();NbnsFixture::failDevice="en1";assert(!service.begin("ledboard"));const auto attempts=NbnsFixture::sockets.size();
  service.service();assert(NbnsFixture::sockets.size()==attempts);NbnsFixture::failDevice.clear();pakarTestMillis+=1000;service.service();assert(service.running());
  const auto replies=NbnsFixture::sent.size();
  assert(NbnsFixture::queue("en1",q,0x08080808));service.service();assert(NbnsFixture::sent.size()==replies); // Never reflects to remote subnets.
  auto oversize=q;oversize.push_back(0);assert(NbnsFixture::queue("en1",oversize,0xc0a80164));service.service();assert(NbnsFixture::sent.size()==replies);
  ETH.address=IPAddress(192,168,1,22);service.service();assert(NbnsFixture::queue("en1",q,0xc0a80164));service.service();assert(NbnsFixture::sent.back().bytes[61]==22);
  service.end();assert(!service.running());
  puts("Actual NBNS codec/service: strict query/NBSTAT, no SMB, exact bound ETH/STA/AP, overlap, rename, link/DHCP, budgets, retry, failures and malformed input passed; not LAN proof.");
}
