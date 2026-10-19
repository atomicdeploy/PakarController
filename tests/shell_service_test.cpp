// The runner injects the ACTUAL production session, initializer and loop here.
// Small clock/socket doubles isolate lease/order behavior, not RF acceptance.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "TelnetShell.h"
#include "ShellTui.h"
#include "ShellWire.h"
using std::min;
#define F(value) value
#define APP_NAME "PakarController"
#define PAKAR_TELNET_SESSIONS 2
#define PAKAR_TELNET_ENABLED 1
static uint32_t clockNow=0,acceptAdvance=1,sendAdvance=0,inputAdvance=0;
static bool concatAllowed=true;
static unsigned inputBytes=0;
uint32_t millis(){return clockNow;}
class String:public std::string {
 public:
  using std::string::string;using std::string::operator=;
  String()=default;String(const std::string& value):std::string(value){}
  template<typename N,typename=std::enable_if_t<std::is_integral_v<N>>> explicit String(N value):std::string(std::to_string(value)){}
  bool concat(const char* bytes,size_t size){if(!concatAllowed)return false;append(bytes,size);return true;}
  void remove(size_t at,size_t size){erase(at,size);}
};
struct Print {
  virtual ~Print()=default;
  virtual size_t write(uint8_t)=0;
  virtual size_t write(const uint8_t*,size_t)=0;
  size_t print(const char* text){return write(reinterpret_cast<const uint8_t*>(text),strlen(text));}
  size_t println(const char* text=""){return print(text)+print("\r\n");}
};
struct ClientState {bool connected=true;std::string input,wire;size_t readAt=0,budget=192;};
static std::vector<std::shared_ptr<ClientState>> clients;
int send(int fd,const char* bytes,size_t count,int){
  auto& client=*clients.at(fd);clockNow+=sendAdvance;
  if(!client.budget){errno=EWOULDBLOCK;return -1;}
  const auto admitted=min(count,client.budget);client.wire.append(bytes,admitted);return int(admitted);
}
constexpr int MSG_DONTWAIT=64,LISTEN=1;
struct WiFiClient {
  std::shared_ptr<ClientState> state;
  bool connected()const{return state&&state->connected;}
  void stop(){if(state)state->connected=false;}
  void setNoDelay(bool){}
  int fd()const{for(size_t i=0;i<clients.size();++i)if(clients[i]==state)return int(i);return -1;}
  size_t availableForWrite()const{return state->budget;}
  int write(const uint8_t* bytes,size_t size){return send(fd(),reinterpret_cast<const char*>(bytes),size,0);}
  size_t available()const{return state->input.size()-state->readAt;}
  int read(){return available()?static_cast<uint8_t>(state->input[state->readAt++]):-1;}
};
struct Server {
  std::deque<WiFiClient> pending;
  bool hasClient()const{return !pending.empty();}
  WiFiClient accept(){auto client=pending.front();pending.pop_front();clockNow+=acceptAdvance;return client;}
  void stop(){} void begin(uint16_t){} void setNoDelay(bool){}
  explicit operator bool()const{return true;}
  int status()const{return LISTEN;}
} telnetServer;
struct Access {
  bool available=true,authentication=false;uint32_t revision=7;
  bool ready()const{return available;}bool enabled()const{return authentication;}
  uint32_t securityRevision()const{return revision;}
} accessConfig;
struct Settings {bool enabled()const{return true;}bool color()const{return true;}uint16_t port()const{return 23;}} shellSettings;
struct Identity {const char* hostname()const{return "ledboard";}} deviceIdentity;
struct Progress {unsigned phase=0;bool known()const{return false;}unsigned percent()const{return 0;}};
namespace PakarUpdate {const char* name(unsigned){return "idle";}}
struct Remote {bool busy()const{return false;}const char* phase()const{return "idle";}Progress progress()const{return {};}} remoteFirmware;
struct Ota {Progress snapshot()const{return {};}} otaProgress;
struct Network {bool stationConnected()const{return false;}} networkConfig;
struct Grid {int state(uint8_t)const{return 0;}bool outputEnabled()const{return false;}} grid;
constexpr int kCellRed=1,kCellOff=2;
bool apStarted=false,avrOnline=false;
/*__PRODUCTION_SESSION__*/
std::unique_ptr<DeviceShellSession> telnetSessions[PAKAR_TELNET_SESSIONS],serialShell;
WiFiClient shellPeers[PAKAR_TELNET_SESSIONS];
PakarShell::SerialChannel shellChannel;
uint32_t shellRebindAt=0,lastShellObserve=0;
uint16_t shellListeningPort=0;
bool shellListening=true;
String shellObserved;
#if defined(ARDUINO_ARCH_ESP32)
std::atomic<bool> shellCloseRequested{false};
#else
bool shellCloseRequested=false;
#endif
void startDiscovery(){}
void retireDeviceShells(){for(unsigned i=0;i<PAKAR_TELNET_SESSIONS;++i){shellPeers[i].stop();telnetSessions[i].reset();}serialShell.reset();shellChannel.close();}
void shellPrompt(DeviceShellSession& s){s.editor.setPrompt(deviceIdentity.hostname(),s.color);s.editor.prompt([&](const uint8_t* p,size_t n){s.write(p,n);});}
void shellTuiDraw(DeviceShellSession&){}
void shellTuiInput(DeviceShellSession&,uint8_t,bool){}
/*__PRODUCTION_INITIALIZER__*/
void inputDeviceShell(DeviceShellSession& s,uint8_t){
  ++inputBytes;clockNow+=inputAdvance;inputAdvance=0;s.lastInput=millis();
  // Cooperative work may service an explicit framed-USB poll while another
  // terminal's input is being handled. The serial lease must use a fresh clock.
  if(serialShell)serialShell->lastInput=millis();
}
/*__PRODUCTION_SERVICE__*/
void reset(uint32_t now=100){
  retireDeviceShells();clients.clear();telnetServer.pending.clear();clockNow=lastShellObserve=now;
  shellRebindAt=0;shellListening=true;shellCloseRequested=false;shellObserved.clear();
  acceptAdvance=1;sendAdvance=inputAdvance=0;concatAllowed=true;inputBytes=0;accessConfig=Access{};
}
WiFiClient pending(const char* input=""){
  auto state=std::make_shared<ClientState>();state->input=input;clients.push_back(state);
  WiFiClient client;client.state=state;telnetServer.pending.push_back(client);return client;
}
void serial(){serialShell=std::make_unique<DeviceShellSession>();initializeDeviceShell(*serialShell,false);shellChannel.open(9);}
int main(){
  reset();auto first=pending("status\r");serviceDeviceShells();
  assert(telnetSessions[0]&&first.connected());
  assert(first.state->wire.find(APP_NAME)!=std::string::npos&&inputBytes==7);
  auto second=pending();serviceDeviceShells();
  assert(telnetSessions[0]&&telnetSessions[1]&&second.connected());
  reset();serial();auto client=pending("q");inputAdvance=7;sendAdvance=2;serviceDeviceShells();
  assert(client.connected()&&telnetSessions[0]&&serialShell&&shellChannel.active());
  assert(telnetSessions[0]->lastDrain==101+sendAdvance);
  assert(serialShell->lastInput==clockNow);

  reset(UINT32_MAX-15);acceptAdvance=32;client=pending();serviceDeviceShells();
  assert(clockNow==16&&client.connected()&&telnetSessions[0]); // Accept crosses millis rollover.
  reset(UINT32_MAX-20);client=pending();serviceDeviceShells();clockNow=15;serviceDeviceShells();
  assert(client.connected()&&telnetSessions[0]); // Existing session crosses rollover.

  reset();client=pending();serviceDeviceShells();clockNow=telnetSessions[0]->lastInput+1800000U;serviceDeviceShells();
  assert(client.connected());++clockNow;serviceDeviceShells();assert(!client.connected()&&!telnetSessions[0]);
  reset();accessConfig.authentication=true;client=pending();serviceDeviceShells();clockNow=telnetSessions[0]->lastInput+60001U;serviceDeviceShells();
  assert(!client.connected()&&!telnetSessions[0]); // Login deadline is unchanged.
  reset();client=pending();client.state->budget=0;serviceDeviceShells();clockNow=telnetSessions[0]->lastDrain+30000U;serviceDeviceShells();
  assert(client.connected());++clockNow;serviceDeviceShells();assert(!client.connected()); // Genuine stalled output still retires.
  reset();serial();clockNow=serialShell->lastInput+30000U;serviceDeviceShells();assert(serialShell);
  ++clockNow;serviceDeviceShells();assert(!serialShell&&!shellChannel.active());

  reset();concatAllowed=false;client=pending();serviceDeviceShells();assert(!client.connected()&&!telnetSessions[0]);
  reset();client=pending();serviceDeviceShells();telnetSessions[0]->fault=true;serviceDeviceShells();assert(!client.connected());
  reset();client=pending();serviceDeviceShells();++accessConfig.revision;serviceDeviceShells();assert(!client.connected());
  reset();serial();accessConfig.available=false;serviceDeviceShells();assert(!serialShell&&!shellChannel.active());
  reset();client=pending();serviceDeviceShells();client.stop();serviceDeviceShells();assert(!telnetSessions[0]);
  std::cout<<"Actual shell session/initializer/service: late accept, two peers, serial refresh, drain, rollover, deadlines and fault/auth retirement passed.\n";
}
