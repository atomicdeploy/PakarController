// Actual production connection/pump with libssh API + FreeRTOS queue doubles.
// This tests admission/draining, NOT cryptography or a physical ESP connection.
#include <Arduino.h>
#include <Preferences.h>
#define LIBSSH_STATIC
#include <libssh_esp32.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <AccessPolicy.h>
#include <cassert>
#include <deque>
#include <string>
#include <vector>
#include <functional>
#define private public
#include "../firmware/esp32s3/src/SshTransport.h"
#undef private
using T=PakarS3::SshTransport;
struct Queue {std::deque<T::Packet> packets;unsigned capacity=32;};
struct ssh_message_struct {int type,subtype;std::string text;};
static Queue incoming,outgoing;
static std::deque<ssh_message_struct> messages;
static uint32_t tick=0;
static unsigned rejected=0,accepted=0,dispatches=0,exitCalls=0;
static int exitCode=-1;static bool inputEof=true,connected=true,blocked=false,revoked=false;
static std::string wire;static std::function<void()> application;
static auto channel=reinterpret_cast<ssh_channel>(1);
static auto session=reinterpret_cast<ssh_session>(1);
uint32_t millis(){return tick;}
void vTaskDelay(unsigned n){tick+=n;if(application)application();assert(tick<40000);}
BaseType_t xQueueSend(QueueHandle_t q,const void* p,unsigned){auto& v=*static_cast<Queue*>(q);if(v.packets.size()==v.capacity)return 0;v.packets.push_back(*static_cast<const T::Packet*>(p));return pdTRUE;}
BaseType_t xQueueReceive(QueueHandle_t q,void* p,unsigned){auto& v=*static_cast<Queue*>(q);if(v.packets.empty())return 0;*static_cast<T::Packet*>(p)=v.packets.front();v.packets.pop_front();return pdTRUE;}
unsigned uxQueueSpacesAvailable(QueueHandle_t q){auto& v=*static_cast<Queue*>(q);return v.capacity-v.packets.size();}
unsigned uxQueueMessagesWaiting(QueueHandle_t q){return static_cast<Queue*>(q)->packets.size();}
extern "C" {
void ssh_set_blocking(ssh_session,int){}
int ssh_handle_key_exchange(ssh_session){return SSH_OK;}
int ssh_is_connected(ssh_session){return connected;}
ssh_message ssh_message_get(ssh_session){return messages.empty()?nullptr:&messages.front();}
int ssh_message_type(ssh_message m){return m->type;}
int ssh_message_subtype(ssh_message m){return m->subtype;}
const char* ssh_message_auth_user(ssh_message){return "admin";}
const char* ssh_message_auth_password(ssh_message m){return m->text.c_str();}
int ssh_message_auth_reply_success(ssh_message,int){return SSH_OK;}
int ssh_message_auth_set_methods(ssh_message,int){return SSH_OK;}
ssh_channel ssh_message_channel_request_open_reply_accept(ssh_message){return channel;}
ssh_channel ssh_message_channel_request_channel(ssh_message){return channel;}
int ssh_message_channel_request_pty_width(ssh_message){return 120;}
int ssh_message_channel_request_pty_height(ssh_message){return 40;}
const char* ssh_message_channel_request_command(ssh_message m){return m->text.c_str();}
int ssh_message_channel_request_reply_success(ssh_message){++accepted;return SSH_OK;}
int ssh_message_reply_default(ssh_message){++rejected;return SSH_OK;}
void ssh_message_free(ssh_message){messages.pop_front();}
int ssh_channel_is_open(ssh_channel){return true;}
int ssh_channel_is_eof(ssh_channel){return inputEof;}
int ssh_channel_read_nonblocking(ssh_channel,void*,uint32_t,int){return 0;}
int ssh_channel_write(ssh_channel,const void* bytes,uint32_t n){if(blocked)return SSH_AGAIN;const auto count=n>3?3:n;wire.append(static_cast<const char*>(bytes),count);return count;}
int ssh_channel_request_send_exit_status(ssh_channel,int code){assert(outgoing.packets.empty());++exitCalls;exitCode=code;return SSH_OK;}
int ssh_channel_close(ssh_channel){return SSH_OK;}
int ssh_blocking_flush(ssh_session,int){return SSH_OK;}
void ssh_channel_free(ssh_channel){}
}
static void reset(T& transport){
  incoming={};outgoing={};messages.clear();wire.clear();tick=rejected=accepted=dispatches=exitCalls=0;exitCode=-1;inputEof=connected=true;blocked=revoked=false;
  transport.inbound_=&incoming;transport.outbound_=&outgoing;transport.desired_=true;transport.securityRevision_=7;transport.config_.epoch=transport.epoch_.load();transport.config_.revision=7;
  strcpy(transport.config_.user,"admin");strcpy(transport.config_.password,"correct-password");
  messages.push_back({SSH_REQUEST_AUTH,SSH_AUTH_METHOD_PASSWORD,"correct-password"});
  messages.push_back({SSH_REQUEST_CHANNEL_OPEN,SSH_CHANNEL_SESSION,{}});
}
int main(){
  T transport;bool sawPty=false;
  application=[&]{T::Packet p;while(transport.receive(p)){
    if(p.kind==T::Exec){++dispatches;sawPty=p.pty;assert(std::string(reinterpret_cast<char*>(p.bytes),p.size)==(sawPty?"tui":"status"));
      const uint8_t bytes[]="result\r\n";assert(transport.output(p.token,p.revision,bytes,sizeof(bytes)-1));transport.finish(p.token,sawPty?1:0);}
  }if(blocked&&tick>60)blocked=false;};
  reset(transport);blocked=true;messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,"status"});
  transport.connection(session);assert(dispatches==1&&wire=="result\r\n"&&exitCode==0&&exitCalls==1&&!sawPty&&tick>60);
  reset(transport);messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_PTY,{}});messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,"tui"});
  transport.connection(session);assert(dispatches==1&&sawPty&&wire=="result\r\n"&&exitCode==1);
  reset(transport);messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,"status"});messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,"reset"});
  transport.connection(session);assert(dispatches==1&&rejected==1&&wire=="result\r\n"&&exitCalls==1);
  for(const auto& bad:std::vector<std::string>{"",std::string(129,'x'),"status\nreset"}){
    reset(transport);messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,bad});transport.connection(session);assert(!dispatches&&rejected&&!exitCalls&&wire.empty());
  }
  reset(transport);messages.front().text="wrong-password";messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,"status"});
  transport.connection(session);assert(!dispatches&&rejected&&!exitCalls);
  reset(transport);application=[&]{if(!revoked&&transport.active()){transport.invalidate();revoked=true;}};
  messages.push_back({SSH_REQUEST_CHANNEL,SSH_CHANNEL_REQUEST_EXEC,"status"});transport.connection(session);assert(revoked&&wire.empty()&&!exitCalls);
  puts("Actual SSH exec transport: authenticated admission, PTY metadata, input EOF, partial writes/backpressure, one dispatch, drained exit status and revocation passed (API/queue doubles).");
}
