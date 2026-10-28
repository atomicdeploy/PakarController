#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <AccessPolicy.h>
namespace PakarS3 {
// Passive-only, binary, flat /user namespace. Network binding owns deadlines,
// peer-matched data sockets and partial output. No active FTP/bounce/FXP path.
template<class Files> class FtpSession {
 public:
  enum Action {None,Passive,Store,Retrieve,List,Quit,Abort};
  Action action=None;char path[72]{};bool authenticated=false;
  const char* response()const{return reply_;}
  void command(char* line,const char* user,const char* password){
    action=None;reply_[0]=0;
    char* argument=strchr(line,' ');if(argument)*argument++=0;else argument=line+strlen(line);
    for(char* p=line;*p;++p)if(*p>='a'&&*p<='z')*p-=32;
    if(!strcmp(line,"USER")){authenticated=false;user_[0]=0;if(strlen(argument)<sizeof(user_))snprintf(user_,sizeof(user_),"%s",argument);reply("331 Password required\r\n");return;}
    if(!strcmp(line,"PASS")){authenticated=user&&password&&*user&&*password&&BoardCore::Access::equal(user_,user)&&BoardCore::Access::equal(argument,password);reply(authenticated?"230 Logged in\r\n":"530 Authentication failed\r\n");return;}
    if(!strcmp(line,"QUIT")){action=Quit;reply("221 Goodbye\r\n");return;}
    if(!authenticated){reply("530 Log in first\r\n");return;}
    if(!strcmp(line,"NOOP")){reply("200 OK\r\n");return;}
    if(!strcmp(line,"SYST")){reply("215 UNIX Type: L8\r\n");return;}
    if(!strcmp(line,"FEAT")){reply("211-Features\r\n EPSV\r\n PASV\r\n SIZE\r\n UTF8\r\n211 End\r\n");return;}
    if(!strcmp(line,"OPTS")&&!strcmp(argument,"UTF8 ON")){reply("200 UTF8\r\n");return;}
    if(!strcmp(line,"TYPE")){reply(!strcmp(argument,"I")?"200 Binary\r\n":"504 Binary only\r\n");return;}
    if(!strcmp(line,"PWD")){reply("257 \"/user\"\r\n");return;}
    if(!strcmp(line,"CWD")){reply(!strcmp(argument,"/user")||!strcmp(argument,"/user/")||!strcmp(argument,".")?"250 Directory selected\r\n":"550 Public namespace only\r\n");return;}
    if(!strcmp(line,"PASV")||!strcmp(line,"EPSV")){action=Passive;extended=!strcmp(line,"EPSV");return;}
    if(!strcmp(line,"ABOR")){action=Abort;reply("226 Aborted\r\n");return;}
    if(!strcmp(line,"LIST")||!strcmp(line,"NLST")){if(*argument&&strcmp(argument,"/user")&&strcmp(argument,"/user/")&&strcmp(argument,".")){reply("550 Directory unavailable\r\n");return;}namesOnly=!strcmp(line,"NLST");action=List;return;}
    if(!strcmp(line,"STOR")||!strcmp(line,"RETR")||!strcmp(line,"SIZE")){
      char name[65];if(!Files::name(argument,name)){reply("550 Invalid public filename\r\n");return;}
      snprintf(path,sizeof(path),"/user/%s",name);
      if(!strcmp(line,"SIZE")){typename Files::Info info;if(!files_.stat(path,info)){reply("550 Not found\r\n");return;}snprintf(reply_,sizeof(reply_),"213 %lu\r\n",static_cast<unsigned long>(info.size));return;}
      action=!strcmp(line,"STOR")?Store:Retrieve;return;
    }
    reply("502 Unsupported command\r\n");
  }
  explicit FtpSession(Files& files):files_(files){}
  bool extended=false,namesOnly=false;
 private:
  Files& files_;char user_[33]{},reply_[192]{};
  void reply(const char* value){snprintf(reply_,sizeof(reply_),"%s",value);}
};
}
