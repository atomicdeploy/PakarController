#include "../firmware/esp/src/AvrIsp.h"
#include <cassert>
#include <cstdio>
static unsigned serviceCalls=0;
static void service(){++serviceCalls;}
static unsigned progressCalls=0,programProgressCalls=0;
static unsigned resetReleases=0;
static bool resetHeld=false;
static void bootJournalOnRelease(uint8_t pin,int level){
 if(pin!=5)return;
 if(level==LOW){resetHeld=true;return;}
 if(!resetHeld)return;
 resetHeld=false;++resetReleases;
 // Old firmware advances a boot journal on every return. The new firmware's
 // exact durable Prog latch deliberately prevents that write during repair.
 if(SPI.eeprom[1008]!=0x50 || SPI.eeprom[1009]!=0x4b || SPI.eeprom[1011]!=1)
   ++SPI.eeprom[416];
}
static void progress(const char *phase,uint32_t done,uint32_t total){
 assert(done && done<=total && !(done%128));++progressCalls;
 if(!strcmp(phase,"program_avr")){assert(done==SPI.flashPages*128);++programProgressCalls;}
}
struct Fixture {
 fs::FS fs;AvrIsp isp{5};
 Fixture(bool eesave=false){
   SPI=FakeSPI{};SPI.eesave=eesave;
   for(unsigned i=0;i<SPI.eeprom.size();++i)SPI.eeprom[i]=i*17+3;
   for(unsigned i=0;i<SPI.flash.size();++i)SPI.flash[i]=i*11+9;
   assert(isp.backup(fs,"/backup"));
 }
 File image() {
   auto d=std::make_shared<File::Data>();d->bytes.resize(32768);
   for(unsigned i=0;i<32768;++i)d->bytes[i]=i*7+2;
   return File(d);
 }
 File recovery(){return fs.open("/backup/eeprom.bin","r");}
 File originalFlash(){return fs.open("/backup/flash.bin","r");}
 File armed(){return fs.open("/backup/eeprom-armed.bin","w+");}
};
int main(){
 {Fixture f;f.isp.setServiceCallback(service);f.isp.setProgressCallback(progress);
  assert(f.isp.backup(f.fs,"/callback"));assert(serviceCalls>0 && progressCalls==528);
  f.isp.setServiceCallback(nullptr);const auto count=serviceCalls;
  auto image=f.image(),eep=f.recovery();assert(f.isp.programFlash(image,eep));assert(serviceCalls==count && programProgressCalls==256);}
 for(bool eesave:{false,true}){
   Fixture f(eesave);const auto before=SPI.eeprom;
   auto image=f.image(),eep=f.recovery();assert(f.isp.programFlash(image,eep));
   assert(SPI.erased==1&&SPI.flashPages==256&&SPI.eeprom==before);
   assert(eesave?SPI.eepromWrites==0:SPI.eepromWrites>0);
   for(unsigned i=0;i<32768;++i)assert(SPI.flash[i]==static_cast<uint8_t>(i*7+2));
 }
 {Fixture f;auto image=f.image(),eep=f.recovery();SPI.eeprom[0]^=1;
  assert(!f.isp.programFlash(image,eep)&&!SPI.erased);}
 {Fixture f;auto image=f.image();File absent;
  assert(!f.isp.programFlash(image,absent)&&!SPI.erased);}
 {Fixture f;auto image=f.image(),eep=f.recovery();SPI.lock=0xFC;
  assert(!f.isp.programFlash(image,eep)&&!SPI.erased);}
 {Fixture f;auto image=f.image(),eep=f.recovery();SPI.badSignature=true;
  assert(!f.isp.programFlash(image,eep)&&!SPI.erased);}
 {Fixture f;auto image=f.image(),eep=f.recovery();SPI.badEeprom=true;
  assert(!f.isp.programFlash(image,eep)&&SPI.erased==1&&SPI.flashPages==0);}
 {Fixture f;const auto before=SPI.eeprom;auto image=f.image(),eep=f.recovery();SPI.badFlash=true;
  assert(!f.isp.programFlash(image,eep)&&SPI.eeprom==before);}
 {Fixture f;const auto old=f.fs.files["/backup/flash.bin"]->bytes;
  f.fs.failWrite=true;assert(!f.isp.backup(f.fs,"/backup")&&!SPI.erased);
  assert(f.fs.files["/backup/flash.bin"]->bytes==old);}
 {Fixture f;auto d=std::make_shared<File::Data>();d->bytes.resize(32768);d->readLimit=32760;
  File image(d);auto eep=f.recovery();assert(!f.isp.programFlash(image,eep)&&!SPI.erased);}
 // Explicit ISP-only recovery: preserve the original files, arm exactly the
 // five existing Prog bytes, and make that readback-verified file the restore.
 for(uint8_t crystal:{uint8_t(0xF7),uint8_t(0xFF)})for(bool eesave:{false,true}){
  Fixture f(eesave);const auto before=SPI.eeprom;
  SPI.lfuse=crystal;
  const auto original=f.fs.files["/backup/eeprom.bin"]->bytes;
  const auto flashBefore=f.fs.files["/backup/flash.bin"]->bytes;
  auto image=f.image(),eep=f.recovery(),flash=f.originalFlash(),armed=f.armed();
  assert(f.isp.programFlash(image,eep,&flash,&armed));
  assert(SPI.erased==1 && SPI.flashPages==256 && SPI.fuseWrites==0);
  const auto stored=f.fs.files["/backup/eeprom-armed.bin"];
  assert(stored->bytes.size()==1024 && stored->flushes==1);
  const uint8_t prog[]={0x50,0x4B,1,1,static_cast<uint8_t>(0x50^0x4B^1^1^0xA5)};
  for(unsigned i=0;i<1024;++i){
    const auto expected=i>=1008 && i<1013?prog[i-1008]:before[i];
    assert(SPI.eeprom[i]==expected && stored->bytes[i]==expected);
  }
  assert(f.fs.files["/backup/eeprom.bin"]->bytes==original);
  assert(f.fs.files["/backup/flash.bin"]->bytes==flashBefore);
 }
 {Fixture f;auto image=f.image(),eep=f.recovery(),flash=f.originalFlash();
  assert(!f.isp.programFlash(image,eep,&flash,nullptr)&&!SPI.erased);}
 {Fixture f;auto image=f.image(),eep=f.recovery(),armed=f.armed();
  assert(!f.isp.programFlash(image,eep,nullptr,&armed)&&!SPI.erased);}
 {Fixture f;auto image=f.image(),eep=f.recovery(),flash=f.originalFlash();
  assert(!f.isp.programFlash(image,eep,&flash,&eep)&&!SPI.erased);}
 for(unsigned mismatch:{0U,32767U}){
  Fixture f;auto image=f.image(),eep=f.recovery(),flash=f.originalFlash(),armed=f.armed();
  SPI.flash[mismatch]^=1;
  assert(!f.isp.programFlash(image,eep,&flash,&armed)&&!SPI.erased);
  assert(armed.size()==0 && !SPI.fuseWrites);
 }
 {Fixture f;auto image=f.image(),eep=f.recovery(),flash=f.originalFlash(),armed=f.armed();
  f.fs.files["/backup/flash.bin"]->readLimit=32767;
  assert(!f.isp.programFlash(image,eep,&flash,&armed)&&!SPI.erased && armed.size()==0);}
 for(unsigned badFuse=0;badFuse<3;++badFuse){
  Fixture f;auto image=f.image(),eep=f.recovery(),flash=f.originalFlash(),armed=f.armed();
  if(badFuse==0)SPI.lfuse=0xFE;
  else SPI.hfuse=badFuse==1?0xDE:0xCF;
  assert(!f.isp.programFlash(image,eep,&flash,&armed)&&!SPI.erased);
  assert(armed.size()==0 && !SPI.fuseWrites);
 }
 for(unsigned storageFailure=0;storageFailure<4;++storageFailure){
  Fixture f;const auto before=SPI.eeprom;
  const auto original=f.fs.files["/backup/eeprom.bin"]->bytes;
  auto image=f.image(),eep=f.recovery(),flash=f.originalFlash(),armed=f.armed();
  auto data=f.fs.files["/backup/eeprom-armed.bin"];
  if(storageFailure==0)armed=File();
  if(storageFailure==1)data->writeLimit=1023;
  if(storageFailure==2)data->readLimit=1023;
  if(storageFailure==3)data->corruptOnFlush=true;
  assert(!f.isp.programFlash(image,eep,&flash,&armed)&&!SPI.erased);
  assert(SPI.eeprom==before && !SPI.eepromWrites && !SPI.fuseWrites);
  assert(f.fs.files["/backup/eeprom.bin"]->bytes==original);
 }
 {Fixture f;auto image=f.image(),eep=f.recovery(),flash=f.originalFlash(),armed=f.armed();
  SPI.eeprom[1023]^=1;
  assert(!f.isp.programFlash(image,eep,&flash,&armed)&&!SPI.erased && armed.size()==0);}
 // Reproduce the real incident: separate backup and program pulse RESET twice,
 // so a boot-journal byte changes after capture. The exact guard must refuse.
 {Fixture f;resetHeld=false;resetReleases=0;pakarTestPinWrite=bootJournalOnRelease;
  assert(f.isp.backup(f.fs,"/separate"));
  auto image=f.image(),eep=f.fs.open("/separate/eeprom.bin","r"),flash=f.originalFlash(),armed=f.armed();
  assert(!f.isp.programFlash(image,eep,&flash,&armed) && !SPI.erased && resetReleases==2);
  pakarTestPinWrite=nullptr;
 }
 for(bool eesave:{false,true}){
  Fixture f(eesave);resetHeld=false;resetReleases=0;pakarTestPinWrite=bootJournalOnRelease;
  // A prior boot legitimately changed the journal since the original archive.
  // The pinned original flash must still match; fresh EEPROM is captured under
  // the one continuously held RESET, rather than comparing to stale settings.
  ++SPI.eeprom[416];const auto freshEeprom=SPI.eeprom;
  const auto oldFlash=f.fs.files["/backup/flash.bin"]->bytes;
  const auto oldEeprom=f.fs.files["/backup/eeprom.bin"]->bytes;
  auto image=f.image(),flash=f.originalFlash();
  assert(f.isp.recoverFlash(f.fs,"/fresh",image,flash));
  assert(resetReleases==1 && !resetHeld && SPI.erased==1 && !SPI.fuseWrites);
  assert(f.fs.files["/fresh/eeprom.bin"]->bytes==std::vector<uint8_t>(freshEeprom.begin(),freshEeprom.end()));
  assert(f.fs.files["/fresh/flash.bin"]->bytes==oldFlash);
  assert(f.fs.files["/backup/flash.bin"]->bytes==oldFlash && f.fs.files["/backup/eeprom.bin"]->bytes==oldEeprom);
  const auto armed=f.fs.files["/fresh/eeprom-armed.bin"]->bytes;
  for(unsigned i=0;i<1024;++i) {
    assert(SPI.eeprom[i]==armed[i]);
    if(i<1008 || i>=1013)assert(armed[i]==freshEeprom[i]);
  }
  auto again=f.image(),original=f.originalFlash();const auto releases=resetReleases;
  assert(!f.isp.recoverFlash(f.fs,"/fresh",again,original) && resetReleases==releases && SPI.erased==1);
  pakarTestPinWrite=nullptr;
 }
 // Every failed held-session preflight releases RESET exactly once and leaves
 // the old archive intact. An interrupted fresh directory is not reusable.
 for(unsigned failure=0;failure<5;++failure){
  Fixture f;resetHeld=false;resetReleases=0;pakarTestPinWrite=bootJournalOnRelease;
  const auto old=f.fs.files["/backup/eeprom.bin"]->bytes;
  auto image=f.image(),flash=f.originalFlash();
  if(failure==0)f.fs.failWrite=true;
  if(failure==1)SPI.flash[32767]^=1;
  if(failure==2)f.fs.files["/backup/flash.bin"]->readLimit=32767;
  if(failure==3)SPI.lfuse=0xFE;
  if(failure==4)f.fs.corruptMetadata=true;
  assert(!f.isp.recoverFlash(f.fs,"/failed",image,flash));
  assert(resetReleases==1 && !resetHeld && !SPI.erased && !SPI.fuseWrites);
  assert(f.fs.files["/backup/eeprom.bin"]->bytes==old);
  pakarTestPinWrite=nullptr;
 }
 {Fixture f;resetHeld=false;resetReleases=0;pakarTestPinWrite=bootJournalOnRelease;
  File absent;auto flash=f.originalFlash();
  assert(!f.isp.recoverFlash(f.fs,"/invalid",absent,flash) && !resetHeld && !resetReleases && !SPI.erased);
  pakarTestPinWrite=nullptr;
 }
 puts("Actual AvrIsp: exact stale-snapshot refusal, one continuous RESET lease across fresh durable flash/EEPROM/fuse capture and programming, old archive retention, exact five-byte Prog arming and all pre-erase/error cleanup checks passed (SPI emulator, not hardware).");
}
