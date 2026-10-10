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
 puts("Actual AvrIsp driver: EESAVE on/off preservation, durable snapshot, locked/wrong target, storage/EEPROM/flash failures passed (SPI emulator, not hardware).");
}
