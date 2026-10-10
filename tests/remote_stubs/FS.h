#pragma once
#include "Arduino.h"
#include <map>
#include <memory>
#include <vector>
enum SeekMode{SeekSet};
struct FSInfo{uint32_t totalBytes=2097152,usedBytes=0;};
class File{
 public:struct Data{std::vector<uint8_t>bytes;};
 File()=default;File(std::shared_ptr<Data>d,bool bad):d_(d),bad_(bad){}
 explicit operator bool()const{return bool(d_);}
 size_t size()const{return d_?d_->bytes.size():0;}
 bool available()const{return d_&&at_<size();}
 bool seek(size_t at,SeekMode){if(!d_||at>size())return false;at_=at;return true;}
 size_t read(uint8_t*p,size_t n){if(!d_)return 0;n=min(n,size()-at_);memcpy(p,d_->bytes.data()+at_,n);at_+=n;return n;}
 int read(){uint8_t p;return read(&p,1)?p:-1;}
 size_t readBytes(char*p,size_t n){return read(reinterpret_cast<uint8_t*>(p),n);}
 size_t write(const uint8_t*p,size_t n){if(!d_||bad_)return 0;if(at_+n>size())d_->bytes.resize(at_+n);memcpy(d_->bytes.data()+at_,p,n);at_+=n;return n;}
 size_t write(uint8_t p){return write(&p,1);}
 void close(){d_.reset();}void flush(){}
 private:std::shared_ptr<Data>d_;size_t at_=0;bool bad_=false;
};
namespace fs{class FS{
 public:std::map<std::string,std::shared_ptr<File::Data>>files;bool failWrite=false,failRename=false;
 bool mkdir(const char*){return true;}
 bool info(FSInfo &i){i.totalBytes=2097152;i.usedBytes=0;for(auto&e:files)i.usedBytes+=e.second->bytes.size();return true;}
 File open(const String&path,const char*mode){if(mode[0]=='w')files[path]=std::make_shared<File::Data>();auto it=files.find(path);return it==files.end()?File():File(it->second,failWrite);}
 bool rename(const String&a,const String&b){if(failRename)return false;auto it=files.find(a);if(it==files.end())return false;files[b]=it->second;files.erase(it);return true;}
};}
