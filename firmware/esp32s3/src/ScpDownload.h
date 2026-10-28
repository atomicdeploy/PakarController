#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

namespace PakarS3 {
// A bounded single-file SCP source, not a shell command interpreter. File reads
// are supplied by the existing loop-owned public UserFileStore, never raw FS.
class ScpDownload {
 public:
  enum Phase {Idle,ReadyAck,Header,HeaderAck,Data,FinalAck,Done,Failed};
  static bool command(const char* text,char* name,size_t capacity){
    constexpr const char* prefix="scp -f /user/";
    if(!text||strncmp(text,prefix,strlen(prefix)))return false;
    const char* p=text+strlen(prefix);const size_t length=strnlen(p,65);
    if(!length||length>64||capacity<=length||p[0]=='.'||strstr(p,".."))return false;
    for(size_t i=0;i<length;++i){const char c=p[i];
      if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
    }
    memcpy(name,p,length+1);return true;
  }
  bool start(const char* name,uint32_t size){
    char commandText[90],checked[65];snprintf(commandText,sizeof(commandText),"scp -f /user/%s",name?name:"");
    if(!command(commandText,checked,sizeof(checked))||size>128U*1024U){phase_=Failed;return false;}
    snprintf(name_,sizeof(name_),"%s",checked);size_=size;offset_=0;phase_=ReadyAck;return true;
  }
  void input(uint8_t byte){
    if(byte){phase_=Failed;return;}
    switch(phase_){case ReadyAck:phase_=Header;break;case HeaderAck:phase_=Data;break;case FinalAck:phase_=Done;break;default:phase_=Failed;}
  }
  // At most one bounded read/output per application loop. Rejected output
  // advances nothing, so queue backpressure never loses or duplicates bytes.
  template<class Read,class Emit> void service(Read read,Emit emit){
    uint8_t bytes[128];
    if(phase_==Header){
      const int count=snprintf(reinterpret_cast<char*>(bytes),sizeof(bytes),"C0644 %lu %s\n",static_cast<unsigned long>(size_),name_);
      if(count<=0||size_t(count)>=sizeof(bytes)){phase_=Failed;return;}
      if(emit(bytes,size_t(count)))phase_=HeaderAck;
    }else if(phase_==Data){
      if(offset_==size_){bytes[0]=0;if(emit(bytes,1))phase_=FinalAck;return;}
      const size_t count=size_-offset_>sizeof(bytes)?sizeof(bytes):size_-offset_;
      if(!read(offset_,bytes,count)){phase_=Failed;return;}
      if(emit(bytes,count))offset_+=count;
    }
  }
  Phase phase()const{return phase_;}
  uint32_t sent()const{return offset_;}
 private:
  Phase phase_=Idle;char name_[65]{};uint32_t size_=0,offset_=0;
};
}
