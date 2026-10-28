#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <memory>
#include <stdlib.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif
namespace PakarS3 {
// Bounded SFTP v3 subset. This is a subsystem on the authenticated SSH channel,
// NOT a second crypto implementation or a mapping to raw LittleFS paths.
template<class Files> class SftpSession {
 public:
  explicit SftpSession(Files& files):files_(files),input_(allocate()){}
  bool ready()const{return bool(input_);}bool failed()const{return failed_;}
  bool input(uint8_t byte){
    if(outSize_||writing_)return false;
    if(failed_||!input_||used_>=capacity){failed_=true;return false;}
    input_[used_++]=byte;
    if(used_==4){length_=get(input_.get());if(length_<5||length_>capacity-4){failed_=true;return true;}}
    if(used_>=4&&used_==length_+4)dispatch();return true;
  }
  template<class Emit> void service(Emit emit){
    if(writing_){
      const size_t count=writeLeft_>512?512:writeLeft_;
      if(count&&!files_.write(writeOffset_,input_.get()+writeAt_,count)){writing_=false;files_.cancel();status(id_,4);}
      else{writeAt_+=count;writeOffset_+=count;writeLeft_-=count;if(!writeLeft_){writing_=false;status(id_,0);}}
    }
    if(outSize_){const size_t n=outSize_-outAt_>128?128:outSize_-outAt_;if(emit(output_+outAt_,n)){outAt_+=n;if(outAt_==outSize_){outSize_=outAt_=used_=0;}}}
  }
 private:
  static constexpr size_t capacity=33024;
  struct Free {void operator()(uint8_t* p)const{free(p);}};
  static uint8_t* allocate(){
#if defined(ARDUINO_ARCH_ESP32)
    return static_cast<uint8_t*>(heap_caps_malloc_prefer(capacity,2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
#else
    return static_cast<uint8_t*>(malloc(capacity));
#endif
  }
  Files& files_;std::unique_ptr<uint8_t[],Free> input_;uint8_t output_[1536]{};
  size_t used_=0,length_=0,pos_=0,outSize_=0,outAt_=0,writeAt_=0,writeLeft_=0;
  uint32_t id_=0,writeOffset_=0;bool initialized_=false,failed_=false,writing_=false,directory_=false;unsigned directoryOffset_=0;
  static uint32_t get(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
  uint32_t number(){if(pos_+4>used_){failed_=true;return 0;}const auto n=get(input_.get()+pos_);pos_+=4;return n;}
  uint64_t wide(){const uint64_t hi=number();return hi<<32|number();}
  bool text(char* value,size_t capacity){const uint32_t n=number();if(failed_||n>=capacity||pos_+n>used_||memchr(input_.get()+pos_,0,n)){failed_=true;return false;}memcpy(value,input_.get()+pos_,n);value[n]=0;pos_+=n;return true;}
  void put(uint32_t n){if(outSize_+4>sizeof(output_)){failed_=true;return;}output_[outSize_++]=n>>24;output_[outSize_++]=n>>16;output_[outSize_++]=n>>8;output_[outSize_++]=n;}
  void bytes(const void* p,size_t n){if(outSize_+n>sizeof(output_)){failed_=true;return;}memcpy(output_+outSize_,p,n);outSize_+=n;}
  void string(const char* s){put(strlen(s));bytes(s,strlen(s));}
  void start(uint8_t type,uint32_t id){outSize_=outAt_=0;put(0);bytes(&type,1);put(id);}
  void end(){const uint32_t n=outSize_-4;output_[0]=n>>24;output_[1]=n>>16;output_[2]=n>>8;output_[3]=n;}
  void status(uint32_t id,uint32_t code){start(101,id);put(code);string(code?"Request rejected":"");string("");end();}
  bool handle(){char h[8];return text(h,sizeof(h))&&!strcmp(h,directory_?"d":"f");}
  void attrs(const typename Files::Info& info,bool dir=false){put(1|4|8);put(0);put(info.size);put(dir?0040755:0100644);put(info.modified/1000);put(info.modified/1000);}
  bool attributes(bool opening=false){
    const uint32_t flags=number();if(flags&~15U)return false;
    if((flags&1)&&(!opening||wide()!=0))return false;
    if(flags&2)return false; // No user/group ownership emulation.
    if(flags&4){const uint32_t mode=number();
      // Creation attributes pass through the store's fixed safe permissions
      // (like a server umask). A later chmod is not falsely acknowledged.
      if(opening){if(mode&~0100777U)return false;}
      else if(mode!=0644&&mode!=0100644)return false;
    }
    if(flags&8){number();const uint32_t modified=number();files_.modified(uint64_t(modified)*1000);}
    return !failed_;
  }
  void dispatch(){
    pos_=4;const uint8_t type=input_[pos_++];id_=number();
    if(type==1){if(initialized_){failed_=true;return;}initialized_=true;start(2,3);end();return;}
    if(!initialized_){failed_=true;return;}
    char path[96];
    if(type==16){if(!text(path,sizeof(path)))return;char name[65];if(strcmp(path,".")&&strcmp(path,"/")&&strcmp(path,"/user")&&strcmp(path,"/user/")&&!Files::name(path,name)){status(id_,3);return;}
      start(104,id_);put(1);if(!strcmp(path,".")||!strcmp(path,"/")||!strcmp(path,"/user")||!strcmp(path,"/user/"))string("/user");else{char full[72];snprintf(full,sizeof(full),"/user/%s",name);string(full);}string("");put(0);end();return;}
    if(type==3){
      if(!text(path,sizeof(path)))return;const uint32_t flags=number();
      if(directory_||files_.opened()||!(flags==1||flags==26||flags==58)||!files_.open(path,flags!=1,flags&32)){status(id_,4);return;}
      if(!attributes(true)){files_.cancel();status(id_,8);return;}start(102,id_);string("f");end();return;
    }
    if(type==4){if(!handle()){status(id_,4);return;}const bool ok=directory_?true:files_.close();directory_=false;status(id_,ok?0:4);return;}
    if(type==6){
      if(!handle()||directory_||!files_.writing()){status(id_,4);return;}
      const uint64_t offset=wide();const uint32_t count=number();
      if(failed_||offset>Files::limit||count>32768||count>Files::limit-offset||pos_+count!=used_){status(id_,4);return;}
      writeOffset_=offset;writeAt_=pos_;writeLeft_=count;writing_=true;return;
    }
    if(type==5){
      if(!handle()||directory_||files_.writing()){status(id_,4);return;}const uint64_t offset=wide();size_t count=number();
      if(failed_||offset>Files::limit||!count){status(id_,4);return;}if(offset>=files_.current().size){status(id_,1);return;}
      uint8_t bytesRead[512];if(!files_.read(offset,bytesRead,count)){status(id_,4);return;}start(103,id_);put(count);bytes(bytesRead,count);end();return;
    }
    if(type==17||type==7||type==8){
      typename Files::Info info;
      if(type!=8){if(!text(path,sizeof(path)))return;
        if(!strcmp(path,"/")||!strcmp(path,"/user")||!strcmp(path,"/user/")){start(105,id_);attrs(info,true);end();return;}
        if(!files_.stat(path,info)){status(id_,2);return;}}
      else{if(!handle()||directory_||!files_.opened()){status(id_,4);return;}info=files_.current();}
      start(105,id_);attrs(info);end();return;
    }
    if(type==10){if(!handle()||directory_||!files_.writing()){status(id_,4);return;}status(id_,attributes()?0:8);return;}
    if(type==11){if(!text(path,sizeof(path)))return;if(directory_||files_.opened()||(strcmp(path,".")&&strcmp(path,"/")&&strcmp(path,"/user")&&strcmp(path,"/user/"))){status(id_,3);return;}directory_=true;directoryOffset_=0;start(102,id_);string("d");end();return;}
    if(type==12){if(!handle()||!directory_){status(id_,4);return;}typename Files::Info rows[8];unsigned count=0;if(!files_.list(directoryOffset_,rows,count)){status(id_,4);return;}
      if(!count){status(id_,1);return;}start(104,id_);put(count);for(unsigned i=0;i<count;++i){string(rows[i].name);string(rows[i].name);attrs(rows[i]);}directoryOffset_+=count;end();return;}
    status(id_,8); // Explicit unsupported: links, rename, remove, directories,
    // arbitrary permissions, random-access edits, resume and vendor extensions.
  }
};
}
