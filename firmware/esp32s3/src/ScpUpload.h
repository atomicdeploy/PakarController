#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
namespace PakarS3 {
// Legacy SCP sink: exact /user/name only, no recursive/shell command execution.
class ScpUpload {
 public:
  enum Phase {Idle,Initial,Header,HeaderAck,Data,Terminator,Commit,Final,Done,Failed};
  static bool command(const char* text,char* name,size_t capacity){
    if(!text||strncmp(text,"scp -t /user/",13))return false;
    const char* p=text+13;size_t n=strnlen(p,65);
    if(!n||n>64||n>=capacity||p[0]=='.'||strstr(p,".."))return false;
    for(size_t i=0;i<n;++i){const char c=p[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_'))return false;}
    memcpy(name,p,n+1);return true;
  }
  void start(const char* name){snprintf(name_,sizeof(name_),"%s",name);phase_=Initial;}
  template<class Files> bool input(uint8_t byte,Files& files){
    if(phase_==Header){
      if(byte!='\n'){if(used_==sizeof(header_)-1||byte<32||byte>126){phase_=Failed;return true;}header_[used_++]=char(byte);return true;}
      header_[used_]=0;bool valid=used_>7&&header_[0]=='C'&&header_[5]==' ';
      for(unsigned i=1;i<=4&&valid;++i)valid=header_[i]>='0'&&header_[i]<='7';
      size_t at=6;uint32_t size=0;bool digit=false;
      while(valid&&header_[at]>='0'&&header_[at]<='9'){digit=true;const unsigned n=header_[at++]-'0';if(size>(Files::limit-n)/10)valid=false;else size=size*10+n;}
      if(!valid||!digit||header_[at]!=' '||strcmp(name_,header_+at+1)||!files.open(name_,true)){phase_=Failed;return true;}
      size_=size;used_=0;phase_=HeaderAck;return true;
    }
    if(phase_==Data){
      if(used_==sizeof(chunk_))return false;
      chunk_[used_++]=byte;if(offset_+used_==size_)phase_=Terminator;return true;
    }
    if(phase_==Terminator){if(byte){phase_=Failed;return true;}phase_=Commit;return true;}
    return false;
  }
  template<class Files,class Emit> void service(Files& files,Emit emit){
    const uint8_t ack=0;
    if(phase_==Initial){if(emit(&ack,1))phase_=Header;}
    else if(phase_==HeaderAck){if(emit(&ack,1))phase_=size_?Data:Terminator;}
    else if((phase_==Data&&used_==sizeof(chunk_))||phase_==Commit){
      if(used_){if(!files.write(offset_,chunk_,used_)){phase_=Failed;return;}offset_+=used_;used_=0;return;}
      if(phase_==Commit)phase_=files.close()?Final:Failed;
    }else if(phase_==Final){if(emit(&ack,1))phase_=Done;}
    if(phase_==Failed){files.cancel();static const uint8_t error[]={1,'U','p','l','o','a','d',' ','f','a','i','l','e','d','\n'};if(emit(error,sizeof(error)))reported_=true;}
  }
  Phase phase()const{return phase_;}bool reported()const{return reported_;}
 private:
  Phase phase_=Idle;char name_[65]{},header_[96]{};uint8_t chunk_[512]{};
  size_t used_=0;uint32_t size_=0,offset_=0;bool reported_=false;
};
}
