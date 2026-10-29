// Host boundary doubles only. The runner inserts actual production functions.
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#define F(x) x
#define APP_NAME "PakarController"
#define HEX 16
using std::min;
static uint32_t now=100;uint32_t millis(){return now;}
class String:public std::string {
 public:
  using std::string::string;using std::string::operator=;
  String()=default;String(const std::string& s):std::string(s){}
  template<class N,typename=std::enable_if_t<std::is_integral_v<N>>> explicit String(N n):std::string(std::to_string(n)){}
  bool concat(const char* b,size_t n){append(b,n);return true;}
  size_t write(uint8_t b){push_back(char(b));return 1;}
  size_t write(const uint8_t* b,size_t n){append(reinterpret_cast<const char*>(b),n);return n;}
  void remove(size_t at,size_t n){erase(at,n);}
  int indexOf(char c,size_t at=0)const{const auto p=find(c,at);return p==npos?-1:int(p);}
  String substring(size_t a,size_t b=npos)const{return substr(a,b==npos?b:b-a);}
  bool startsWith(const char* p)const{return rfind(p,0)==0;}
  bool endsWith(const char* p)const{const auto n=strlen(p);return size()>=n&&compare(size()-n,n,p)==0;}
  void trim(){const auto a=find_first_not_of(" \t\r\n");if(a==npos){clear();return;}erase(0,a);erase(find_last_not_of(" \t\r\n")+1);}
  void toLowerCase(){for(char& c:*this)c=std::tolower(static_cast<unsigned char>(c));}
};
struct Print {
 virtual ~Print()=default;virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;
 template<class V>size_t print(const V& v){std::ostringstream s;s<<v;const auto b=s.str();return write(reinterpret_cast<const uint8_t*>(b.data()),b.size());}
 template<class V>size_t println(const V& v){return print(v)+println();}
 size_t println(){return print("\r\n");}
 size_t println(uint32_t v,int){std::ostringstream s;s<<std::hex<<v;return println(s.str());}
};
#include <ArduinoJson.h>
#include "TelnetShell.h"
#include "ShellTui.h"
#include "ShellWire.h"
#include "ShellProperties.h"
#include "ShellPixelCommand.h"
#include "ShellToneCommand.h"
#include "../../firmware/esp32s3/src/SshPolicy.h"
struct {uint32_t revision=7;bool available=true;bool ready()const{return available;}bool enabled()const{return true;}uint32_t securityRevision()const{return revision;}} accessConfig;
struct {bool color()const{return true;}} shellSettings;
struct {const char* hostname()const{return "keycontroller";}} deviceIdentity;
struct {bool stationConnected()const{return true;}} networkConfig;
namespace NetworkPath {bool ready(){return true;}}
struct Layout {static constexpr unsigned groupCount=80;uint32_t revision()const{return 1;}unsigned used()const{return 480;}unsigned start(unsigned id)const{return (id-1)*6;}unsigned count(unsigned)const{return 6;}bool diagnosticRange(unsigned i,unsigned n)const{return i+n<=480;}};
struct {String outputJson()const{return "{\"enabled\":true}";}Layout groupLayout()const{return {};}} grid;
[[maybe_unused]]struct {String json()const{return "{}";}} keySettings;
unsigned winner=0;const char* hostname="keycontroller";bool avrOnline=true,apStarted=true;
static bool busy=false,commandAccepted=true,blocked=false;static unsigned dispatched=0,tuiDraws=0;
bool shellBusy(){return busy;}
String shellGet(const ShellProperty& p){return p.secret?"<write-only>":"keycontroller";}
bool shellSet(const String&,const String&,String& r){r="saved";return commandAccepted;}
bool handleCloudCommand(const String&,String& r){++dispatched;r=commandAccepted?"accepted":"AVR busy";return commandAccepted;}
bool handleMaintenanceShell(const String&,String& r){r="info";return commandAccepted;}
bool submitDisplayText(const String&,uint32_t,uint32_t,const String&,bool,uint32_t,String&){return commandAccepted;}
bool submitNetworkProbe(const String&,const String&,String&){return commandAccepted;}
String networkToolsJson(){return "{}";}String deviceNameJson(){return "{}";}String wifiScanJson(bool){return "{}";}
constexpr unsigned kUnsupported=1,kNoError=0;
struct {unsigned scan(){return kNoError;}unsigned reconnect(){return kNoError;}} espPanelSettings;
/*__SESSION__*/
/*__COMMANDS__*/
void shellTuiDraw(DeviceShellSession& s,bool){++tuiDraws;s.print("TUI frame\r\n");}
struct ShellTuiSnapshot {PakarShell::ShellTui::Snapshot view;explicit ShellTuiSnapshot(DeviceShellSession& s){view.columns=s.parser.columns();view.lines=s.parser.rows();}};
/*__TUI_INPUT__*/
void inputDeviceShell(DeviceShellSession& s,uint8_t c){if(s.tui.active())shellTuiInput(s,c,false);}
namespace PakarS3 {
struct SshTransport {
 enum Kind:uint8_t {Open,Input,Resize,Output,Close,FileRead,FileWrite,Sftp,Exec,InputEnd};
 struct Packet {uint32_t token=9,revision=7;uint16_t width=120,height=40;uint8_t kind=Exec,size=0;bool pty=false;uint8_t bytes[128]{};};
 std::deque<Packet> packets;std::string wire;uint32_t token=9;int exit=-1;
 void service(bool,uint32_t){}const char* error(){return "";}
 template<class... A>void service(A...){}
 uint32_t active(){return token;}bool listening(){return false;}uint16_t port(){return 22;}
 bool receive(Packet& p){if(packets.empty())return false;p=packets.front();packets.pop_front();return true;}
 bool output(uint32_t t,uint32_t r,const uint8_t* b,size_t n){assert(t==9&&r==7);if(blocked)return false;wire.append(reinterpret_cast<const char*>(b),n);return true;}
 void close(uint32_t){token=0;}void finish(uint32_t,unsigned code=0){exit=code;}
};
}
// The service calls credentials on the global access owner; use a facade for
// those members without changing the production extraction.
struct AccessFacade {bool ready(){return accessConfig.ready();}uint32_t securityRevision(){return accessConfig.securityRevision();}const char* username(){return "admin";}const char* password(){return "not-printed";}} accessFacade;
std::unique_ptr<DeviceShellSession> sshShell;uint32_t sshShellToken=0;uint16_t sshAnnouncedPort=0;
PakarS3::SshTransport deviceSsh;void startDiscovery(){}
#define accessConfig accessFacade
/*__SSH_LOOP__*/
#undef accessConfig
static void run(const char* command,bool pty=false){
 sshShell.reset();deviceSsh={};dispatched=tuiDraws=0;busy=blocked=false;commandAccepted=true;accessConfig.revision=7;accessConfig.available=true;
 PakarS3::SshTransport::Packet p;p.pty=pty;p.size=strlen(command);memcpy(p.bytes,command,p.size);deviceSsh.packets.push_back(p);serviceSshShell();
 for(unsigned i=0;i<80&&deviceSsh.exit<0&&sshShell&&!sshShell->tui.active();++i)serviceSshShell();
}
int main(){
 run("status");assert(deviceSsh.exit==0&&deviceSsh.wire.find("Host keycontroller")!=std::string::npos&&deviceSsh.wire.find(APP_NAME)==std::string::npos&&deviceSsh.wire.find('\x1b')==std::string::npos);
 run("get hostname");assert(deviceSsh.exit==0&&deviceSsh.wire=="keycontroller\r\n");
 for(const char* cmd:{"unknown","get missing","beep 0","terminal size 0 0","tui","edit"}){run(cmd);assert(deviceSsh.exit==1&&deviceSsh.wire.find("ERR ")==0&&tuiDraws==0);}
 run("beep 440 100");assert(dispatched==1&&deviceSsh.exit==0);
 run("get hostname");deviceSsh.wire.clear();deviceSsh.exit=-1;sshShell->closing=false;sshShell->output="held output\r\n";blocked=true;
 serviceSshShell();assert(deviceSsh.wire.empty()&&deviceSsh.exit<0&&sshShell->output=="held output\r\n");
 blocked=false;serviceSshShell();assert(deviceSsh.wire=="held output\r\n"&&deviceSsh.exit==0);
 run("tui",true);assert(sshShell->tui.active()&&tuiDraws==1&&deviceSsh.exit<0&&sshShell->parser.columns()==120);
 PakarS3::SshTransport::Packet p;p.kind=PakarS3::SshTransport::Resize;p.width=100;p.height=30;deviceSsh.packets.push_back(p);serviceSshShell();assert(sshShell->parser.columns()==100&&tuiDraws>=2);
 const auto frames=tuiDraws;p.kind=PakarS3::SshTransport::InputEnd;deviceSsh.packets.push_back(p);
 p.kind=PakarS3::SshTransport::Resize;deviceSsh.packets.push_back(p);serviceSshShell();for(unsigned i=0;i<10&&deviceSsh.exit<0;++i)serviceSshShell();assert(deviceSsh.exit==0&&tuiDraws==frames&&deviceSsh.wire.find("\x1b[?1049l")!=std::string::npos);
 run("edit",true);assert(sshShell->tui.active()&&tuiDraws==1);
 shellTuiInput(*sshShell,27,false);shellTuiInput(*sshShell,0,true);assert(!sshShell->tui.active()&&sshShell->closing);
 const auto before=deviceSsh.wire;serviceSshShell();for(unsigned i=0;i<10&&deviceSsh.exit<0;++i)serviceSshShell();assert(deviceSsh.exit==0&&deviceSsh.wire.size()>=before.size()&&deviceSsh.wire.find("\x1b[?1049l")!=std::string::npos);
 // Actual dispatcher: errors become nonzero; queries remain available during
 // maintenance; rejected commands are neither reinterpreted nor replayed.
 auto& s=*sshShell;s.tui=PakarShell::ShellTui{};s.commandOnly=true;s.terminal=false;s.closing=false;s.color=false;s.output.clear();busy=true;
 executeDeviceShell(s,"beep 440 100");assert(s.exitStatus==1&&!dispatched);
 executeDeviceShell(s,"status");assert(s.exitStatus==0&&!dispatched);
 busy=false;commandAccepted=false;s.output.clear();executeDeviceShell(s,"beep 440 100");assert(s.exitStatus==1&&dispatched==1);
 ++accessConfig.revision;executeDeviceShell(s,"beep 440 100");assert(s.exitStatus==1&&dispatched==1&&s.closing);
 std::cout<<"Actual SSH shell exec binding/dispatcher: plain output, failures, PTY TUI/resize/EOF, aliases, maintenance, AVR rejection and stale-auth guards passed.\n";
}
