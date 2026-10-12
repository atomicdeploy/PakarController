#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Actual PU850 LocalLib/MNDP.cpp TLV layout; bounds enforced before each copy.
// Standard four-byte header: reserved/version bytes then big-endian sequence.
namespace PakarMndp {
class Packet {
 public:
  explicit Packet(uint16_t sequence) { bytes_[2]=sequence>>8; bytes_[3]=sequence; }
  bool tlv(uint16_t type,const void *value,size_t length) {
    if(failed_ || !value || length>255 || size_+4+length>sizeof(bytes_)) {failed_=true;return false;}
    bytes_[size_++]=type>>8;bytes_[size_++]=type;
    bytes_[size_++]=length>>8;bytes_[size_++]=length;
    memcpy(bytes_+size_,value,length);size_+=length;return true;
  }
  bool text(uint16_t type,const char *value) {return value && tlv(type,value,strlen(value));}
  const uint8_t *data() const {return bytes_;}
  size_t size() const {return failed_ ? 0 : size_;}
 private:
  uint8_t bytes_[512]{};
  size_t size_=4;bool failed_=false;
};
}
