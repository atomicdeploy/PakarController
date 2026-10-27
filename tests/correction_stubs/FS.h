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
  size_t readBytes(char *out,size_t n){size_t got=0;for(;got<n;++got){int c=read();if(c<0)break;out[got]=char(c);}return got;}
  size_t print(const String &value){if(!data||failWrite)return 0;*data=value;return value.size();}
  String readString(){return data?String(*data):String();}
  void close(){data.reset();}
};
namespace fs {
class FS {
 public:
  std::map<std::string,std::shared_ptr<std::string>> files;
  unsigned writes=0;bool failWrite=false,failRename=false;
  bool exists(const char *path)const{return files.count(path);}
  File open(const char *path,const char *mode){
    if(mode[0]=='w'){++writes;files[path]=std::make_shared<std::string>();}
    return {exists(path)?files[path]:nullptr,0,failWrite};
  }
  bool rename(const char *from,const char *to){if(failRename||!exists(from))return false;files[to]=files[from];files.erase(from);return true;}
};
}
