#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

namespace PakarShell {
// Adapted PU850 LocalLib/RemoteCommand.cpp listener lifecycle, datagram receive
// and reply. No broadcast, truncated command dispatch, global shell session or
// unconditional success. This explicitly opt-in surface is PUBLIC READ-ONLY.
class RemoteQuery {
 public:
  static constexpr uint16_t port=9876;
  bool listening()const{return listening_;}
  template<class Socket> void stop(Socket& socket){socket.stop();listening_=false;}
  template<class Socket,class PublicStatus>
  void service(Socket& socket,uint32_t now,uint32_t networkEpoch,bool enabled,PublicStatus status){
    if(!enabled||!networkEpoch){if(listening_)stop(socket);epoch_=0;return;}
    if(epoch_!=networkEpoch){if(listening_)stop(socket);epoch_=networkEpoch;attempted_=false;}
    if(!listening_){
      if(attempted_&&uint32_t(now-lastBind_)<5000)return;
      attempted_=true;lastBind_=now;listening_=socket.begin(port)!=0;
      if(!listening_)return;
    }
    const int size=socket.parsePacket();if(size<=0)return;
    // One bounded packet per loop; discard remainder without interpreting it.
    if(size>64||(haveReply_&&uint32_t(now-lastReply_)<250)){socket.flush();return;}
    char input[65]{};const int received=socket.read(reinterpret_cast<uint8_t*>(input),size);
    if(received!=size){socket.flush();return;}
    const auto address=socket.remoteIP();const auto replyPort=socket.remotePort();
    if(!replyPort)return;
    for(int i=0;i<size;++i)if(input[i]<32||input[i]>126)return;
    // A fixed 16-hex nonce lets callers correlate replies without a session.
    if(size<18||input[16]!=' ')return;
    for(unsigned i=0;i<16;++i)if(!((input[i]>='0'&&input[i]<='9')||(input[i]>='a'&&input[i]<='f')||(input[i]>='A'&&input[i]<='F')))return;
    char output[384]{};input[16]=0;
    const char* command=input+17;
    int written=0;
    if(!strcmp(command,"hello")||!strcmp(command,"status")){
      char value[320]{};status(value,sizeof(value));
      // Callback only supplies public identity/health, never stored config.
      for(char* p=value;*p;++p)if(*p<32||*p>126)*p='?';
      written=snprintf(output,sizeof(output),"%s OK %s",input,value);
    }else written=snprintf(output,sizeof(output),"%s ERR read-only: hello | status; use authenticated Telnet/API for commands",input);
    if(written<=0||static_cast<size_t>(written)>=sizeof(output))return;
    haveReply_=true;lastReply_=now;
    if(socket.beginPacket(address,replyPort)){socket.write(reinterpret_cast<const uint8_t*>(output),written);socket.endPacket();}
  }
 private:
  uint32_t epoch_=0,lastBind_=0,lastReply_=0;
  bool listening_=false,attempted_=false,haveReply_=false;
};
}
