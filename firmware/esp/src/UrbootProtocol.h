#pragma once
#include <stdint.h>
#include <stddef.h>

// Independent implementation of the published urprotocol wire specification:
// https://github.com/stefanrueger/urboot/blob/main/urprotocol.md
// Deliberately NOT STK500/Arduino: modern Urboot uses byte addresses, short
// lengths and device-specific response bytes. No chip erase,
// boot-region write, fuse command or write retry exists here.
namespace PakarUrboot {
template<class Port> class Protocol {
 public:
  explicit Protocol(Port &port):port_(port){}
  const char *error() const {return error_;}
  uint8_t features() const {return features_;}
  bool eepromWritable() const {return valid_ && eeprom_;}
  bool connect() {
    valid_=false;eeprom_=false;error_="";
    // One external reset is owned by the caller. Only read-only sync is retried,
    // within the bootloader's one-second window; never reset in a retry loop.
    uint8_t previous[2]={};unsigned consistent=0;
    for(unsigned attempt=0;attempt<8;++attempt) {
      for(unsigned drain=0;drain<512 && port_.available()>0;++drain)if(port_.read()<0)break;
      const uint8_t request[]={0x30,0x20};uint8_t reply[2];
      if(!send(request,2))return false;
      if(!receive(reply,2,60)) {consistent=0;continue;}
      if(reply[0]==reply[1] || (reply[0]==0x14 && reply[1]==0x10))
        return fail("Not the supported Urboot protocol");
      const uint32_t info=uint32_t(reply[0])*255U+(reply[1]<reply[0]?reply[1]:reply[1]-1U);
      if(info%2040U!=119U || !((info/2040U)&4U))return fail("Urboot MCU/read capability mismatch");
      if(reply[0]==previous[0] && reply[1]==previous[1])++consistent;
      else {previous[0]=reply[0];previous[1]=reply[1];consistent=1;}
      if(consistent>=3) {
        in_=reply[0];ok_=reply[1];features_=info/2040U;valid_=true;error_="";
        uint8_t metadata[6];
        if(!read(false,32762,metadata,6))return false;
        // Current reviewed stock MiniCore 384-byte vector/autobaud/EEPROM
        // profile only. A custom/larger bootloader requires explicit review.
        if(metadata[0]!=3 || metadata[1]!=25 || metadata[5]!=0x40 ||
            metadata[4]!=0xe7U || features_!=20U)
          return fail("Unsupported Urboot allocation/features; no flash write");
        eeprom_=(metadata[4]&0x40U)!=0;
        return true;
      }
    }
    return fail("Urboot did not synchronize; no flash write");
  }
  bool read(bool eeprom,uint16_t address,uint8_t *out,uint8_t length) {
    if(!valid_ || !out || !length || length>128 ||
       uint32_t(address)+length>(eeprom?1024U:32768U))return fail("Invalid Urboot read bounds");
    const uint8_t request[]={uint8_t(eeprom?1:3),uint8_t(address),uint8_t(address>>8),length,0x20};
    uint8_t marker;
    if(!send(request,5)||!receive(&marker,1)||marker!=in_ ||
       !receive(out,length)||!receive(&marker,1)||marker!=ok_)
      return fail("Urboot read response invalid; operation stopped");
    return true;
  }
  bool program(uint16_t address,const uint8_t *page) {
    if(!valid_ || !page || address%128 || address>=32384 || (features_&8U))
      return fail("Urboot refuses boot-region/partial/NOR page write");
    const uint8_t header[]={2,uint8_t(address),uint8_t(address>>8),128},end=0x20;
    uint8_t reply[2];
    if(!send(header,4)||!send(page,128)||!send(&end,1)||!receive(reply,2,150)||
        reply[0]!=in_||reply[1]!=ok_)
      return fail("Urboot write outcome unknown; NOT retried");
    return true;
  }
  void leave() {
    if(valid_) {const uint8_t request[]={0x51,0x20};send(request,2);}
    valid_=false;
  }
  // UR_PROG_PAGE_EE uses byte addresses. Keep chunks below the AVR SRAM
  // bound and allow EEPROM's slower byte programming, not flash-page timing.
  // A failed ACK invalidates this session; the caller must never replay it.
  bool programEeprom(uint16_t address,const uint8_t *bytes,uint8_t length) {
    if(!eepromWritable() || !bytes || !length || length>128 ||
       uint32_t(address)+length>1024U)return fail("Invalid/unsupported EEPROM write");
    const uint8_t header[]={0,uint8_t(address),uint8_t(address>>8),length},end=0x20;
    uint8_t reply[2];
    if(!send(header,4)||!send(bytes,length)||!send(&end,1)||
       !receive(reply,2,100U+uint32_t(length)*10U)||reply[0]!=in_||reply[1]!=ok_)
      return fail("EEPROM write outcome unknown; NOT retried");
    uint8_t actual[128];
    if(!read(true,address,actual,length))return false;
    for(uint8_t i=0;i<length;++i)if(actual[i]!=bytes[i])
      return fail("EEPROM readback mismatch; NOT retried");
    return true;
  }
 private:
  bool fail(const char *error){error_=error;valid_=false;return false;}
  bool send(const uint8_t *data,size_t size) {
    return port_.write(data,size)==size || fail("Short UART write; NOT retried");
  }
  bool receive(uint8_t *out,size_t size,uint32_t limit=100) {
    const uint32_t start=port_.now();size_t at=0;
    while(at<size && uint32_t(port_.now()-start)<limit) {
      if(port_.available()>0) {const int value=port_.read();if(value>=0)out[at++]=uint8_t(value);else port_.idle();}
      else port_.idle();
    }
    return at==size;
  }
  Port &port_;const char *error_="";
  bool valid_=false,eeprom_=false;uint8_t in_=0,ok_=0,features_=0;
};
}
