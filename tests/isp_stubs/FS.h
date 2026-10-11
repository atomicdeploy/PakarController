#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
#include <vector>
enum SeekMode {SeekSet};
class File {
 public:
  struct Data {std::vector<uint8_t> bytes;size_t readLimit=SIZE_MAX;};
  File()=default;
  File(std::shared_ptr<Data> data,bool fail=false):d_(data),fail_(fail){}
  explicit operator bool()const{return bool(d_);}
  size_t size()const{return d_?d_->bytes.size():0;}
  bool seek(size_t at,SeekMode){if(!d_||at>size())return false;at_=at;return true;}
  size_t read(uint8_t *out,size_t n){
    if(!d_||at_>=d_->readLimit)return 0;
    n=min(n,min(size()-at_,d_->readLimit-at_));memcpy(out,d_->bytes.data()+at_,n);at_+=n;return n;
  }
  int read(){uint8_t b;return read(&b,1)==1?b:-1;}
  size_t write(const uint8_t *p,size_t n){
    if(!d_||fail_)return 0;
    if(at_+n>size())d_->bytes.resize(at_+n);
    memcpy(d_->bytes.data()+at_,p,n);at_+=n;return n;
  }
  size_t printf(const char *fmt,...){
    char b[256];va_list ap;va_start(ap,fmt);int n=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);
    return write(reinterpret_cast<const uint8_t*>(b),n);
  }
  void flush(){}
  void close(){d_.reset();}
 private:std::shared_ptr<Data>d_;size_t at_=0;bool fail_=false;
};
namespace fs {
class FS {
 public:
  bool failWrite=false;
  std::map<std::string,std::shared_ptr<File::Data>>files;
  void mkdir(const char*){}
  bool rename(const String &a,const String &b) {
    auto it=files.find(a);if(it==files.end())return false;files[b]=it->second;files.erase(it);return true;
  }
  File open(const String &path,const char*mode){
    if(mode[0]=='w') files[path]=std::make_shared<File::Data>();
    auto it=files.find(path);return it==files.end()?File():File(it->second,failWrite);
  }
};
}
