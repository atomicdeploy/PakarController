#include "../firmware/esp8266/src/AvrIsp.h"
#include <cassert>
#include <cstdio>
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
 puts("Actual AvrIsp: legacy EESAVE/snapshot safeguards plus pinned full-flash/fuse ISP recovery, unchanged original backups, exact five-byte Prog arming and pre-erase storage/readback failures passed (SPI emulator, not hardware).");
}
