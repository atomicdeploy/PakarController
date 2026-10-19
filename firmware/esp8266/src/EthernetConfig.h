#pragma once
#include <Arduino.h>
#include <FS.h>
#include <IPAddress.h>
#include <stddef.h>
#include <string.h>

// Independent optional Ethernet profile. No hardware is initialized here.
// IPv4 validation adapts NetworkConfigStore::setAddressing; durable storage
// adapts CloudConfigStore's no-op/pending/readback/rename transaction.
class EthernetConfigStore {
 public:
  bool begin(fs::FS &filesystem) {
    filesystem_=&filesystem;defaults();error_=String();
    File file=filesystem.open(kPath,"r");
    if(!file)return true;
    Record stored{};bool complete=false;
    if(file.size()==sizeof(LegacyRecord)) {
      LegacyRecord legacy{};
      complete=file.read(reinterpret_cast<uint8_t*>(&legacy),sizeof(legacy))==sizeof(legacy)&&
          legacy.magic==kMagic&&legacy.layout==1&&legacy.dhcp<=1&&
          legacy.checksum==digest(legacy,offsetof(LegacyRecord,checksum));
      if(complete) {
        stored=record_;stored.dhcp=legacy.dhcp;stored.flags|=1; // Old Ethernet profiles were always active.
        memcpy(stored.addresses,legacy.addresses,sizeof(stored.addresses));
        stored.checksum=checksum(stored);
      }
    } else if(file.size()==sizeof(stored)) {
      complete=file.read(reinterpret_cast<uint8_t*>(&stored),sizeof(stored))==sizeof(stored);
    }
    file.close();
    if(!complete||!valid(stored)) {
      error_=F("Ethernet settings invalid; DHCP defaults loaded without overwriting saved profile");return false;
    }
    record_=stored;return true;
  }
  bool save() {
    if(!filesystem_) {error_=F("LittleFS is not mounted");return false;}
    record_.checksum=checksum(record_);
    if(!valid(record_)) {error_=F("Invalid Ethernet settings; previous profile preserved");return false;}
    File current=filesystem_->open(kPath,"r");Record previous{};
    const bool identical=current&&current.size()==sizeof(previous)&&
        current.read(reinterpret_cast<uint8_t*>(&previous),sizeof(previous))==sizeof(previous)&&
        memcmp(&previous,&record_,sizeof(previous))==0;
    if(current)current.close();
    if(identical) {error_=String();return true;}
    File pending=filesystem_->open(kPending,"w");
    if(!pending) {error_=F("Cannot create Ethernet settings candidate");return false;}
    const bool complete=pending.write(reinterpret_cast<const uint8_t*>(&record_),sizeof(record_))==sizeof(record_);
    pending.close();
    File verify=filesystem_->open(kPending,"r");Record readback{};
    const bool verified=verify&&verify.size()==sizeof(readback)&&
        verify.read(reinterpret_cast<uint8_t*>(&readback),sizeof(readback))==sizeof(readback)&&
        memcmp(&readback,&record_,sizeof(readback))==0;
    if(verify)verify.close();
    if(!complete||!verified||!filesystem_->rename(kPending,kPath)) {
      error_=F("Ethernet settings commit failed; previous profile preserved");return false;
    }
    error_=String();return true;
  }
  bool setAddressing(bool dhcp,const String &address=String(),const String &gateway=String(),
      const String &subnet=String(),const String &dns1=String(),const String &dns2=String()) {
    Record candidate=record_;
    if(!dhcp) {
      const String values[]={address,gateway,subnet,dns1,dns2};
      for(unsigned index=0;index<5;++index) {
        IPAddress ip;
        if(index>=3&&values[index].length()==0) {
          memset(candidate.addresses[index],0,4);continue;
        }
        if(values[index].length()>15||values[index].length()!=strlen(values[index].c_str())||!ip.fromString(values[index])) {
          error_=F("Ethernet address, gateway, subnet and DNS must be IPv4 values");return false;
        }
        for(unsigned byte=0;byte<4;++byte)candidate.addresses[index][byte]=ip[byte];
      }
      if(!validAddresses(candidate.addresses)) {
        error_=F("Invalid Ethernet static subnet, host, gateway or DNS address");return false;
      }
    }
    // DHCP retains the last validated independent static profile.
    candidate.dhcp=dhcp?1:0;candidate.checksum=checksum(candidate);
    record_=candidate;error_=String();return true;
  }
  bool dhcpEnabled()const{return record_.dhcp!=0;}
  bool enabled()const{return (record_.flags&1)!=0;}
  bool lldpEnabled()const{return (record_.flags&2)!=0;}
  bool lldpReceive()const{return (record_.flags&4)!=0;}
  void setServices(bool enabled,bool lldp,bool receive) {
    record_.flags=(enabled?1:0)|(lldp?2:0)|(receive?4:0);
    record_.checksum=checksum(record_);error_=String();
  }
  IPAddress address()const{return IPAddress(record_.addresses[0]);}
  IPAddress gateway()const{return IPAddress(record_.addresses[1]);}
  IPAddress subnet()const{return IPAddress(record_.addresses[2]);}
  IPAddress dns1()const{return IPAddress(record_.addresses[3]);}
  IPAddress dns2()const{return IPAddress(record_.addresses[4]);}
  const char* lastError()const{return error_.c_str();}
 private:
  struct Record {
    uint32_t magic;uint8_t layout,dhcp,flags;uint8_t addresses[5][4];uint32_t checksum;
  } __attribute__((packed));
  struct LegacyRecord {
    uint32_t magic;uint8_t layout,dhcp;uint8_t addresses[5][4];uint32_t checksum;
  } __attribute__((packed));
  static constexpr uint32_t kMagic=0x504B4554UL; // PKET; private persisted layout.
  static constexpr char kPath[]="/ethernet.bin",kPending[]="/ethernet.pending";
  template<class T> static uint32_t digest(const T &record,size_t length) {
    uint32_t result=2166136261U;
    const auto *bytes=reinterpret_cast<const uint8_t*>(&record);
    for(size_t i=0;i<length;++i)result=(result^bytes[i])*16777619U;
    return result;
  }
  static uint32_t checksum(const Record &record) {return digest(record,offsetof(Record,checksum));}
  static bool validAddresses(const uint8_t addresses[5][4]) {
    uint32_t numbers[5]{};
    for(unsigned i=0;i<5;++i)for(unsigned b=0;b<4;++b)numbers[i]=(numbers[i]<<8)|addresses[i][b];
    const uint32_t hostMask=~numbers[2];
    return !(hostMask<3||hostMask==UINT32_MAX||(hostMask&(hostMask+1))||
        (numbers[0]>>24)==0||(numbers[0]>>24)>=224||
        (numbers[0]&hostMask)==0||(numbers[0]&hostMask)==hostMask||
        (numbers[1]>>24)==0||(numbers[1]>>24)>=224||
        (numbers[0]&numbers[2])!=(numbers[1]&numbers[2])||
        (numbers[1]&hostMask)==0||(numbers[1]&hostMask)==hostMask||
        numbers[0]==numbers[1]||(numbers[3]>>24)>=224||(numbers[4]>>24)>=224);
  }
  static bool valid(const Record &record) {
    if(record.magic!=kMagic||record.layout!=2||record.dhcp>1||(record.flags&~7)||record.checksum!=checksum(record))return false;
    bool hasProfile=false;
    for(const auto &ip:record.addresses)for(uint8_t byte:ip)hasProfile=hasProfile||byte!=0;
    return (!hasProfile&&record.dhcp)||validAddresses(record.addresses);
  }
  void defaults() {
    // Compiled support is not permission to drive unwired optional GPIOs.
    // First commissioning enables the port explicitly; preserve old active profiles above.
    memset(&record_,0,sizeof(record_));record_.magic=kMagic;record_.layout=2;record_.dhcp=1;record_.flags=2;
    record_.checksum=checksum(record_);
  }
  fs::FS *filesystem_=nullptr;
  Record record_{};
  String error_;
};
