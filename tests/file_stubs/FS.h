#pragma once
#include "../access_stubs/Arduino.h"
#include <map>
#include <memory>
class File {
 public:
  std::shared_ptr<std::string> data;size_t position=0;bool failWrite=false;
  explicit operator bool()const{return bool(data);}
  size_t size()const{return data?data->size():0;}
  int read(){return data&&position<data->size()?uint8_t((*data)[position++]):-1;}
  size_t read(uint8_t *out,size_t n){size_t i=0;for(;i<n;++i){int c=read();if(c<0)break;out[i]=c;}return i;}
  size_t readBytes(char *out,size_t n){return read(reinterpret_cast<uint8_t*>(out),n);}
  size_t write(const uint8_t *bytes,size_t n){if(!data||failWrite)return 0;if(data->size()<position+n)data->resize(position+n);memcpy(data->data()+position,bytes,n);position+=n;return n;}
  size_t print(const String &s){return write(reinterpret_cast<const uint8_t*>(s.c_str()),s.size());}
  bool seek(size_t offset){if(!data||offset>data->size())return false;position=offset;return true;}
  String readString(){return data?String(*data):String();}
  void flush(){}
  void close(){data.reset();}
};
namespace fs {
class FS {
 public:
  std::map<std::string,std::shared_ptr<std::string>> files;
  unsigned writes=0;bool failWrite=false,failRename=false;
  bool exists(const char *path)const{return files.count(path);}
  bool mkdir(const char *path){files[path]=std::make_shared<std::string>();return true;}
  File open(const char *path,const char *mode){if(mode[0]=='w'){++writes;files[path]=std::make_shared<std::string>();}return {exists(path)?files[path]:nullptr,0,failWrite};}
  bool rename(const char *from,const char *to){if(failRename||!exists(from))return false;files[to]=files[from];files.erase(from);return true;}
  bool remove(const char *path){return files.erase(path)>0;}
};
}
