#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <libssh_esp32.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <AccessPolicy.h>
#include "SshPolicy.h"
#ifndef PAKAR_SCP_ENABLED
#define PAKAR_SCP_ENABLED 0
#endif
#if PAKAR_SCP_ENABLED
#include "ScpDownload.h"
#endif

namespace PakarS3 {
// Transport only. The loop-owned DeviceShellSession remains the sole command,
// TUI and board mutation owner. Adapt libssh's public-domain samplesshd example;
// deliberately do not import demo credentials, logging, WiFi setup or echo loop.
class SshTransport {
 public:
  enum Kind:uint8_t {Open,Input,Resize,Output,Close,FileRead};
  struct Packet {uint32_t token=0,revision=0;uint16_t width=80,height=24;uint8_t kind=Input,size=0;uint8_t bytes[SshPolicy::packetBytes]{};};
  enum Error:uint8_t {None,Settings,Memory,HostKey,Bind,Transport};
  bool begin(){
    Preferences p;if(!p.begin("pakar-ssh",false)){error_=Settings;return false;}
    Record value{};
    if(p.isKey("settings")){
      if(p.getBytesLength("settings")!=sizeof(value)||p.getBytes("settings",&value,sizeof(value))!=sizeof(value)||!valid(value)){p.end();error_=Settings;return false;}
      settings_=value;
    }
    p.end();settingsReady_=true;return true;
  }
  bool configure(bool enabled,uint16_t port,uint16_t telnetPort){
    if(!settingsReady_||!SshPolicy::portAllowed(port,telnetPort))return false;
    Record next=settings_;next.enabled=enabled;next.port=port;next.check=checksum(next);
    if(!memcmp(&next,&settings_,sizeof(next)))return true;
    Preferences p;if(!p.begin("pakar-ssh",false))return false;
    Record verify{};const bool ok=p.putBytes("settings",&next,sizeof(next))==sizeof(next)&&
      p.getBytes("settings",&verify,sizeof(verify))==sizeof(verify)&&!memcmp(&verify,&next,sizeof(next));
    p.end();if(!ok)return false;settings_=next;invalidate();return true;
  }
  bool enabled()const{return settingsReady_&&settings_.enabled;}
  uint16_t port()const{return settings_.port;}
  bool listening()const{return listening_.load();}
  uint32_t active()const{return active_.load();}
  const char* error()const{
    switch(error_.load()){case None:return "";case Settings:return "SSH settings unavailable/corrupt";case Memory:return "SSH memory budget unavailable";
      case HostKey:return "SSH host key unavailable/corrupt";case Bind:return "SSH listener failed";default:return "SSH transport failed";}
  }
  void fingerprint(char* out,size_t size){portENTER_CRITICAL(&metadataLock_);snprintf(out,size,"%s",fingerprint_);portEXIT_CRITICAL(&metadataLock_);}
  bool receive(Packet &p){return inbound_&&xQueueReceive(inbound_,&p,0)==pdTRUE;}
  bool output(uint32_t token,uint32_t revision,const uint8_t* bytes,size_t count){
    if(!outbound_||!SshPolicy::admitted(token,active(),revision,securityRevision_.load())||!count||count>SshPolicy::packetBytes)return false;
    Packet p;p.kind=Output;p.token=token;p.revision=revision;p.size=count;memcpy(p.bytes,bytes,count);return xQueueSend(outbound_,&p,0)==pdTRUE;
  }
  void close(uint32_t token){if(token&&active()==token)closeToken_=token;}
  void finish(uint32_t token,unsigned status=0){if(token&&active()==token){finishStatus_=status;finishToken_=token;}}
  void invalidate(){desired_=false;epoch_.fetch_add(1);active_=0;listening_=false;}
  // Called only from loop. Credential bytes are copied before task creation;
  // asynchronous auth updates can only revoke (never partially rewrite) a worker.
  void service(bool allowed,uint32_t revision,const char* user,const char* password){
    if(revision!=securityRevision_.load()){securityRevision_=revision;invalidate();}
    allowed=allowed&&enabled()&&revision&&user&&password&&BoardCore::Access::usernameValid(user)&&BoardCore::Access::passwordValid(password);
    if(!allowed){if(desired_.load())invalidate();return;}
    if(error_.load()==HostKey||error_.load()==Settings)return;
    if(running_.load()||uint32_t(millis()-lastStart_)<5000U)return;
    lastStart_=millis();
    if(ESP.getFreeHeap()<90000U){error_=Memory;return;}
    if(!inbound_)inbound_=xQueueCreate(8,sizeof(Packet));
    if(!outbound_)outbound_=xQueueCreate(16,sizeof(Packet));
    if(!inbound_||!outbound_){error_=Memory;return;}
    xQueueReset(inbound_);xQueueReset(outbound_);
    config_={};config_.epoch=epoch_.load();config_.revision=revision;config_.port=port();
    snprintf(config_.user,sizeof(config_.user),"%s",user);snprintf(config_.password,sizeof(config_.password),"%s",password);
    desired_=true;running_=true;error_=None;
    if(xTaskCreatePinnedToCore(entry,"pakar-ssh",24576,this,1,nullptr,0)!=pdPASS){running_=false;desired_=false;erase(config_.password,sizeof(config_.password));error_=Memory;}
  }
 private:
  struct Record {uint32_t magic=0x504b5353;uint8_t layout=1,enabled=0;uint16_t port=22;uint32_t check=0;} __attribute__((packed));
  struct Config {uint32_t epoch=0,revision=0;uint16_t port=22;char user[33]{},password[65]{};};
  static uint32_t checksum(const Record&r){uint32_t h=2166136261U;const auto*p=reinterpret_cast<const uint8_t*>(&r);for(size_t i=0;i<offsetof(Record,check);++i){h^=p[i];h*=16777619U;}return h;}
  static bool valid(const Record&r){return r.magic==0x504b5353&&r.layout==1&&r.enabled<=1&&r.port&&r.check==checksum(r);}
  static void erase(void* p,size_t n){auto* b=static_cast<volatile uint8_t*>(p);while(n--)*b++=0;}
  bool current(const Config&c)const{return desired_.load()&&c.epoch==epoch_.load()&&c.revision==securityRevision_.load();}
  bool enqueue(Packet&p){return current(config_)&&xQueueSend(inbound_,&p,0)==pdTRUE;}
  static void entry(void* value){auto*self=static_cast<SshTransport*>(value);self->run();erase(self->config_.password,sizeof(self->config_.password));self->active_=0;self->listening_=false;self->running_=false;vTaskDelete(nullptr);}
  ssh_key hostKey(){
    Preferences p;if(!p.begin("pakar-ssh",false))return nullptr;
    ssh_key key=nullptr;char pem[4096]{};
    if(p.isKey("hostkey")){
      const size_t count=p.getString("hostkey",pem,sizeof(pem));
      if(!count||count>=sizeof(pem)-1||ssh_pki_import_privkey_base64(pem,nullptr,nullptr,nullptr,&key)!=SSH_OK)key=nullptr;
      // Existing malformed keys fail closed; never silently change identity.
    }else if(ssh_pki_generate(SSH_KEYTYPE_ECDSA,256,&key)==SSH_OK){
      char* generated=nullptr;
      if(ssh_pki_export_privkey_base64(key,nullptr,nullptr,nullptr,&generated)!=SSH_OK||!generated||strlen(generated)>=sizeof(pem)-1||
          !p.putString("hostkey",generated)||!p.getString("hostkey",pem,sizeof(pem))||strcmp(generated,pem)){
        ssh_key_free(key);key=nullptr;
      }
      if(generated){erase(generated,strlen(generated));ssh_string_free_char(generated);}
    }
    p.end();erase(pem,sizeof(pem));
    if(key){unsigned char* hash=nullptr;size_t length=0;
      if(ssh_get_publickey_hash(key,SSH_PUBLICKEY_HASH_SHA256,&hash,&length)==SSH_OK){
        char* printable=ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256,hash,length);
        if(printable){portENTER_CRITICAL(&metadataLock_);snprintf(fingerprint_,sizeof(fingerprint_),"%s",printable);portEXIT_CRITICAL(&metadataLock_);ssh_string_free_char(printable);}
        ssh_clean_pubkey_hash(&hash);
      }
    }
    return key;
  }
  void connection(ssh_session session){
    const Config&c=config_;ssh_set_blocking(session,0);
    uint32_t started=millis();int result=SSH_AGAIN;
    while(current(c)&&!SshPolicy::expired(millis(),started,SshPolicy::handshakeMs)&&(result=ssh_handle_key_exchange(session))==SSH_AGAIN)vTaskDelay(pdMS_TO_TICKS(5));
    if(result!=SSH_OK||!current(c))return;
    bool authenticated=false,shell=false;unsigned failures=0;ssh_channel channel=nullptr;
    uint16_t width=80,height=24;started=millis();
    char fileName[65]{};bool fileRead=false;
    while(current(c)&&ssh_is_connected(session)&&!SshPolicy::expired(millis(),started,SshPolicy::loginMs)&&!shell){
      ssh_message message=ssh_message_get(session);
      if(!message){vTaskDelay(pdMS_TO_TICKS(5));continue;}
      bool accepted=false;
      const auto type=ssh_message_type(message),subtype=ssh_message_subtype(message);
      if(type==SSH_REQUEST_AUTH&&!authenticated){
        if(subtype==SSH_AUTH_METHOD_PASSWORD){
          // Always authenticated SSH, independent of HTTP's enabled flag.
          const char* user=ssh_message_auth_user(message);const char* password=ssh_message_auth_password(message);
          authenticated=user&&password&&strnlen(user,33)<=32&&strnlen(password,65)<=64&&
            BoardCore::Access::equal(user,c.user)&&BoardCore::Access::equal(password,c.password);
          if(authenticated&&current(c)){ssh_message_auth_reply_success(message,0);accepted=true;}else ++failures;
        }
        if(!accepted)ssh_message_auth_set_methods(message,SSH_AUTH_METHOD_PASSWORD);
      }else if(authenticated&&type==SSH_REQUEST_CHANNEL_OPEN&&subtype==SSH_CHANNEL_SESSION&&!channel){channel=ssh_message_channel_request_open_reply_accept(message);accepted=channel;}
      else if(authenticated&&channel&&type==SSH_REQUEST_CHANNEL&&ssh_message_channel_request_channel(message)==channel){
        if(subtype==SSH_CHANNEL_REQUEST_PTY){width=SshPolicy::columns(ssh_message_channel_request_pty_width(message));height=SshPolicy::rows(ssh_message_channel_request_pty_height(message));ssh_message_channel_request_reply_success(message);accepted=true;}
        else if(subtype==SSH_CHANNEL_REQUEST_SHELL){ssh_message_channel_request_reply_success(message);accepted=shell=true;}
#if PAKAR_SCP_ENABLED
        else if(subtype==SSH_CHANNEL_REQUEST_EXEC&&ScpDownload::command(ssh_message_channel_request_command(message),fileName,sizeof(fileName))){
          ssh_message_channel_request_reply_success(message);accepted=shell=fileRead=true;
        }
#endif
      }
      if(!accepted)ssh_message_reply_default(message);ssh_message_free(message);
      vTaskDelay(1); // Authentication floods must not starve the core's idle/WDT task.
      if(failures>=3)break;
    }
    if(shell&&current(c)&&nextToken_!=UINT32_MAX){
      const uint32_t token=++nextToken_;active_=token;closeToken_=0;finishToken_=0;
      Packet opening;opening.kind=Open;opening.token=token;opening.revision=c.revision;opening.width=width;opening.height=height;
      if(fileRead){opening.kind=FileRead;opening.size=strlen(fileName);memcpy(opening.bytes,fileName,opening.size);}
      if(enqueue(opening))pump(session,channel,token);
      active_=0;
    }
    if(channel){
      // A nonblocking channel close may itself need socket progress. Bound the
      // final flush instead of freeing an accepted exit-status packet early.
      const uint32_t closing=millis();int result;
      do{result=ssh_channel_close(channel);if(result==SSH_AGAIN)vTaskDelay(pdMS_TO_TICKS(5));}
      while(result==SSH_AGAIN&&current(c)&&!SshPolicy::expired(millis(),closing,2000));
      if(current(c))ssh_blocking_flush(session,100);
      ssh_channel_free(channel);
    }
    // Global login-rate bound is on this worker, never the application loop.
    if(!authenticated)vTaskDelay(pdMS_TO_TICKS(1000));
  }
  void pump(ssh_session session,ssh_channel channel,uint32_t token){
    Packet pending;bool hasPending=false;size_t offset=0;uint32_t lastInput=millis(),lastDrain=millis();
    while(current(config_)&&active()==token&&closeToken_.load()!=token&&ssh_is_connected(session)&&ssh_channel_is_open(channel)&&!ssh_channel_is_eof(channel)){
      if(SshPolicy::expired(millis(),lastInput,SshPolicy::idleMs))break;
      // Reject extra channels, exec/subsystems and forwarding. Only the one
      // authenticated shared shell is supported; resize remains bounded.
      ssh_message message=ssh_message_get(session);
      if(message){bool accepted=false;
        if(ssh_message_type(message)==SSH_REQUEST_CHANNEL&&ssh_message_channel_request_channel(message)==channel&&ssh_message_subtype(message)==SSH_CHANNEL_REQUEST_WINDOW_CHANGE){
          Packet resize;resize.kind=Resize;resize.token=token;resize.revision=config_.revision;
          resize.width=SshPolicy::columns(ssh_message_channel_request_pty_width(message));resize.height=SshPolicy::rows(ssh_message_channel_request_pty_height(message));
          accepted=enqueue(resize);if(accepted)ssh_message_channel_request_reply_success(message);
        }
        if(!accepted)ssh_message_reply_default(message);ssh_message_free(message);
      }
      if(uxQueueSpacesAvailable(inbound_)){
        Packet input;input.token=token;input.revision=config_.revision;
        const int n=ssh_channel_read_nonblocking(channel,input.bytes,sizeof(input.bytes),0);
        if(n>0){input.size=n;lastInput=millis();if(!enqueue(input))break;}
        else if(n==SSH_ERROR)break;
      }
      if(!hasPending&&xQueueReceive(outbound_,&pending,0)==pdTRUE){
        hasPending=SshPolicy::admitted(pending.token,token,pending.revision,config_.revision);offset=0;lastDrain=millis();
      }
      if(hasPending){
        const int n=ssh_channel_write(channel,pending.bytes+offset,pending.size-offset);
        if(n>0){offset+=n;lastDrain=millis();if(offset==pending.size)hasPending=false;}
        else if(n==SSH_ERROR||SshPolicy::expired(millis(),lastDrain,SshPolicy::drainMs))break;
      }
      if(!hasPending&&!uxQueueMessagesWaiting(outbound_)&&finishToken_.load()==token){
        // Deliver an exit status only after all accepted output has drained.
        const int result=ssh_channel_request_send_exit_status(channel,finishStatus_.load());
        if(result==SSH_OK||result==SSH_ERROR)break;
        if(SshPolicy::expired(millis(),lastDrain,SshPolicy::drainMs))break;
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  void run(){
    static bool initialized=false;if(!initialized){libssh_begin();if(ssh_init()!=SSH_OK){error_=Transport;return;}initialized=true;}
    ssh_bind listener=ssh_bind_new();if(!listener){error_=Memory;return;}
    ssh_key key=hostKey();
    if(!key){error_=HostKey;ssh_bind_free(listener);return;}
    bool processConfig=false;int port=config_.port;
    bool imported=false;
    const bool options=ssh_bind_options_set(listener,SSH_BIND_OPTIONS_PROCESS_CONFIG,&processConfig)==SSH_OK&&
      ssh_bind_options_set(listener,SSH_BIND_OPTIONS_BINDADDR,"0.0.0.0")==SSH_OK&&
      ssh_bind_options_set(listener,SSH_BIND_OPTIONS_BINDPORT,&port)==SSH_OK&&
      (imported=ssh_bind_options_set(listener,SSH_BIND_OPTIONS_IMPORT_KEY,key)==SSH_OK);
    if(!imported)ssh_key_free(key); // Successful import transfers ownership.
    ssh_bind_set_blocking(listener,0);
    if(!options||ssh_bind_listen(listener)!=SSH_OK){error_=Bind;ssh_bind_free(listener);return;}
    // In the pinned port ssh_bind_set_blocking changes only a bookkeeping
    // flag; accept() itself still uses a blocking descriptor. An aborted TCP
    // connect between select/accept must not strand revocation/maintenance.
    const int fd=ssh_bind_get_fd(listener);
    const int flags=fd>=0?fcntl(fd,F_GETFL,0):-1;
    if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){error_=Bind;ssh_bind_free(listener);return;}
    listening_=true;
    while(current(config_)){
      const int socket=ssh_bind_get_fd(listener);if(socket<0||socket>=FD_SETSIZE){error_=Transport;break;}
      fd_set readable;FD_ZERO(&readable);FD_SET(socket,&readable);timeval timeout{0,200000};
      const int ready=select(socket+1,&readable,nullptr,nullptr,&timeout);
      if(ready<0&&errno!=EINTR){error_=Transport;break;}
      if(ready<=0){vTaskDelay(pdMS_TO_TICKS(5));continue;}
      if(ESP.getFreeHeap()<60000U){error_=Memory;vTaskDelay(pdMS_TO_TICKS(100));continue;}
      error_=None;
      ssh_session session=ssh_new();if(!session){error_=Memory;break;}
      if(ssh_bind_accept(listener,session)==SSH_OK){connection(session);ssh_disconnect(session);}
      ssh_free(session);vTaskDelay(pdMS_TO_TICKS(20));
    }
    ssh_bind_free(listener);
  }
  Record settings_{};bool settingsReady_=false;uint32_t lastStart_=0,nextToken_=0;
  Config config_{};QueueHandle_t inbound_=nullptr,outbound_=nullptr;
  std::atomic<bool> desired_{false},running_{false},listening_{false};
  std::atomic<uint32_t> epoch_{1},securityRevision_{0},active_{0},closeToken_{0},finishToken_{0},finishStatus_{0};std::atomic<Error> error_{None};
  portMUX_TYPE metadataLock_=portMUX_INITIALIZER_UNLOCKED;char fingerprint_[100]{};
};
}
