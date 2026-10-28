#pragma once
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>
// Protocol-independent adapter over the managed store. Callback remains on the
// main loop; SSH's crypto task only queues bytes. Each instance owns one handle.
template<class Request> class TransferFiles {
 public:
  explicit TransferFiles(Request request):request_(request){}
  static constexpr uint32_t limit=128*1024;
  struct Info {char name[65]{};uint32_t size=0;uint64_t modified=0;char hash[65]{};};
  static bool name(const char* path,char* out){
    if(!path)return false;
    if(!strncmp(path,"/user/",6))path+=6;
    const size_t n=strlen(path);if(!n||n>64||path[0]=='.'||strstr(path,".."))return false;
    for(size_t i=0;i<n;++i){const char c=path[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;}
    snprintf(out,65,"%s",path);return true;
  }
  bool list(unsigned offset,Info* rows,unsigned &count){
    JsonDocument q,r;q["action"]="list";q["offset"]=offset;
    if(!call(q,r))return false;count=0;
    for(auto row:r["files"].template as<JsonArrayConst>()){info(row,rows[count++]);if(count==8)break;}
    return true;
  }
  bool stat(const char* path,Info &out){
    char key[65];if(!name(path,key))return false;
    for(unsigned offset=0;offset<32;offset+=8){Info rows[8];unsigned count=0;if(!list(offset,rows,count))return false;
      for(unsigned i=0;i<count;++i)if(!strcmp(rows[i].name,key)){out=rows[i];return true;}
      if(count<8)break;
    }return false;
  }
  bool open(const char* path,bool write,bool exclusive=false){
    if(open_)return false;char key[65];if(!name(path,key))return false;
    if(!write){if(!stat(path,current_))return false;open_=true;writing_=false;return true;}
    JsonDocument q,r;q["action"]="list";if(!call(q,r))return false;
    const uint32_t revision=r["revision"]|0U;Info old;const bool exists=stat(path,old);
    if(exclusive&&exists)return false;
    q.clear();q["action"]="begin";q["name"]=key;q["size"]=limit;q["modified_ms"]=uint64_t(0);q["revision"]=revision;
    if(exists)q["replace_sha256"]=old.hash;
    if(!call(q,r,true))return false;token_=r["token"]|0U;if(!token_)return false;
    current_={};snprintf(current_.name,sizeof(current_.name),"%s",key);open_=writing_=true;return true;
  }
  bool write(uint32_t offset,const uint8_t* bytes,size_t count){
    if(!open_||!writing_||!count||count>512||offset>limit||count>limit-offset)return false;
    JsonDocument q,r;q["action"]="write";q["token"]=token_;q["offset"]=offset;
    char hex[1025];const char* digits="0123456789abcdef";for(size_t i=0;i<count;++i){hex[2*i]=digits[bytes[i]>>4];hex[2*i+1]=digits[bytes[i]&15];}hex[count*2]=0;q["hex"]=hex;
    if(!call(q,r))return false;current_.size=r["received"]|0U;return true;
  }
  bool read(uint32_t offset,uint8_t* bytes,size_t &count){
    if(!open_||writing_||offset>current_.size)return false;
    if(count>512)count=512;if(count>current_.size-offset)count=current_.size-offset;
    JsonDocument q,r;q["action"]="read";q["name"]=current_.name;q["sha256"]=current_.hash;q["offset"]=offset;q["count"]=uint16_t(count);
    if(!call(q,r))return false;const char* hex=r["hex"]|"";if(strlen(hex)!=count*2)return false;
    for(size_t i=0;i<count;++i){int a=nibble(hex[2*i]),b=nibble(hex[2*i+1]);if(a<0||b<0)return false;bytes[i]=uint8_t(a*16+b);}return true;
  }
  bool close(){
    if(!open_)return false;if(!writing_){open_=false;return true;}
    JsonDocument q,r;q["action"]="commit";q["token"]=token_;q["modified_ms"]=current_.modified;
    if(!call(q,r)||r["committed"]!=true){cancel();return false;}
    snprintf(current_.hash,sizeof(current_.hash),"%s",r["sha256"]|"");cancel();return true;
  }
  void cancel(){if(token_){JsonDocument q,r;q["action"]="cancel";q["token"]=token_;call(q,r);}open_=writing_=false;token_=0;}
  bool opened()const{return open_;}bool writing()const{return writing_;}
  void modified(uint64_t ms){if(open_&&writing_)current_.modified=ms;}
  const Info& current()const{return current_;}
 private:
  Request request_;Info current_;uint32_t token_=0;bool open_=false,writing_=false;
  bool call(JsonDocument&q,JsonDocument&r,bool stream=false){return !deserializeJson(r,request_(q.template as<JsonObjectConst>(),stream))&&r["accepted"]==true;}
  static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
  static void info(JsonVariantConst row,Info &out){out={};snprintf(out.name,sizeof(out.name),"%s",row["name"]|"");out.size=row["size"]|0U;out.modified=row["modified_ms"]|uint64_t(0);snprintf(out.hash,sizeof(out.hash),"%s",row["sha256"]|"");}
};
