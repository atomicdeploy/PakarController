#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// PCController SettingsStore's validated MCU ownership and unchanged-write
// avoidance, with AccessConfig's stage/readback/rename LittleFS transaction.
// This preference is independent of the scene and of transient driver success.
template<class Filesystem> class LedBackendSettings {
 public:
  bool begin(Filesystem &fs,bool mounted,uint8_t fallback) {
    fs_=mounted?&fs:nullptr;selection_=fallback;saved_=false;
    error_=mounted?"":"storage_unavailable";
    if(!mounted||!fs.exists(path()))return false;
    Record value{};
    if(!read(path(),value)){error_="backend_settings_damaged";return false;}
    selection_=value.backend;saved_=true;return true;
  }
  bool save(uint8_t backend) {
    if(backend<1||backend>2){error_="invalid_backend";return false;}
    if(!fs_){error_="storage_unavailable";return false;}
    Record next{0x504b4c42U,1,backend,0,0};next.check=checksum(next);
    Record previous{};
    if(read(path(),previous)&&!memcmp(&previous,&next,sizeof(next))){
      selection_=backend;saved_=true;error_="";return true;
    }
    auto pending=fs_->open(pendingPath(),"w");
    if(!pending){error_="backend_stage_failed";return false;}
    const bool written=pending.write(reinterpret_cast<const uint8_t*>(&next),sizeof(next))==sizeof(next);
    pending.close();Record verify{};
    if(!written||!read(pendingPath(),verify)||memcmp(&next,&verify,sizeof(next))){error_="backend_verify_failed";return false;}
    if(!fs_->rename(pendingPath(),path())){error_="backend_commit_failed";return false;}
    selection_=backend;saved_=true;error_="";return true;
  }
  // Called explicitly at boot before the output worker starts. Persist a fresh
  // board's valid build default once; never repair corruption or format storage.
  bool initializeDefault() {
    if(saved_)return true;
    if(*error_||selection_<1||selection_>2)return false;
    return save(selection_);
  }
  uint8_t selection()const{return selection_;}
  bool saved()const{return saved_;}
  const char* error()const{return error_;}
  static const char* path(){return "/led-backend.bin";}
  static const char* pendingPath(){return "/led-backend.pending";}
 private:
  struct Record{uint32_t magic;uint8_t layout,backend;uint16_t reserved;uint32_t check;};
  static_assert(sizeof(Record)==12,"Private LED backend record size changed");
  static uint32_t checksum(const Record &r){
    uint32_t result=2166136261U;const auto* bytes=reinterpret_cast<const uint8_t*>(&r);
    for(size_t n=0;n<offsetof(Record,check);++n)result=(result^bytes[n])*16777619U;
    return result;
  }
  bool read(const char* name,Record &r){
    auto f=fs_->open(name,"r");const bool okay=f&&f.size()==sizeof(r)&&f.read(reinterpret_cast<uint8_t*>(&r),sizeof(r))==sizeof(r);f.close();
    return okay&&r.magic==0x504b4c42U&&r.layout==1&&r.backend>=1&&r.backend<=2&&!r.reserved&&r.check==checksum(r);
  }
  Filesystem* fs_=nullptr;uint8_t selection_=0;bool saved_=false;const char* error_="";
};
