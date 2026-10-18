#pragma once
// Included in the application's loop-owner namespace, after maintenance APIs.
// PU850 ShellSession/output routing and DeviceParameters are adapted here;
// terminal editing, TUI state and binary ownership remain independently tested.
#ifndef PAKAR_TELNET_SESSIONS
#define PAKAR_TELNET_SESSIONS 2
#endif
#ifndef PAKAR_SERIAL_SHELL_ENABLED
#define PAKAR_SERIAL_SHELL_ENABLED 1
#endif
#ifndef PAKAR_TELNET_ENABLED
#define PAKAR_TELNET_ENABLED 1
#endif
static_assert(PAKAR_TELNET_SESSIONS>=1&&PAKAR_TELNET_SESSIONS<=4,"Shell session count outside1..4");
struct DeviceShellSession : public Print {
  PakarShell::TelnetParser parser;
  PakarShell::LineEditor editor;
  PakarShell::ShellTui tui;
  PakarShell::OutputWindow serialOutput;
  String output;
  uint32_t lastInput=0,lastDrain=0,escapeAt=0;
  uint32_t accessRevision=0;
  bool authenticated=false,closing=false,fault=false,color=true,watch=false,upper=false;
  size_t write(uint8_t byte) override{return write(&byte,1);}
  size_t write(const uint8_t *bytes,size_t size) override {
    if(fault||output.length()+size>8192U){fault=true;return 0;}
    const size_t before=output.length();output.concat(reinterpret_cast<const char*>(bytes),size);
    if(!before&&size)lastDrain=millis();
    if(output.length()!=before+size){fault=true;return 0;}return size;
  }
  using Print::write;
};
std::unique_ptr<DeviceShellSession> telnetSessions[PAKAR_TELNET_SESSIONS],serialShell;
WiFiClient shellPeers[PAKAR_TELNET_SESSIONS];
PakarShell::SerialChannel shellChannel;
uint32_t shellTokenCounter=0,shellRebindAt=0,lastShellObserve=0;
uint16_t shellListeningPort=0;
bool shellListening=false;
String shellObserved;
#if defined(ARDUINO_ARCH_ESP32)
std::atomic<bool> shellCloseRequested{false};
#else
bool shellCloseRequested=false;
#endif

struct ShellProperty {const char *key,*domain,*field;char type;bool writable,secret;};
// Descriptor-driven get/set and TUI share the same typed configuration owners.
const ShellProperty shellProperties[]={
 {"hostname","device","hostname",'s',true,false},{"label","device","display_name",'s',true,false},
 {"version","device","version",'s',false,false},{"boot_id","device","boot_id",'s',false,false},
 {"uptime","device","uptime",'n',false,false},{"heap","device","heap",'n',false,false},
 {"avr.online","device","avr",'b',false,false},{"display.ack","device","display",'b',false,false},
 {"firmware.phase","device","phase",'s',false,false},
 {"network.mode","network","mode",'s',true,false},
 {"network.dhcp","network","dhcp",'b',true,false},{"network.ssid","network","ssid",'s',true,false},
 {"network.password","network","password",'s',true,true},
 {"network.address","network","address",'s',true,false},{"network.gateway","network","gateway",'s',true,false},
 {"network.subnet","network","subnet",'s',true,false},{"network.dns1","network","dns1",'s',true,false},
 {"network.dns2","network","dns2",'s',true,false},{"network.ip","network","station_ip",'s',false,false},
 {"network.ap_ip","network","ap_ip",'s',false,false},
 {"led.enabled","led","enabled",'b',true,false},{"led.mode","led","mode",'s',true,false},
 {"led.brightness","led","brightness",'n',true,false},{"led.effect","led","effect",'s',true,false},
 {"led.backend","driver","backend",'s',false,false},{"led.rmt","driver","rmt",'b',false,false},{"led.dma","driver","dma",'b',false,false},
 {"mqtt.enabled","cloud","mqtt_enabled",'b',true,false},{"mqtt.tls","cloud","mqtt_tls",'b',true,false},
 {"mqtt.host","cloud","mqtt_host",'s',true,false},{"mqtt.port","cloud","mqtt_port",'n',true,false},
 {"mqtt.user","cloud","mqtt_user",'s',true,false},{"mqtt.password","cloud","mqtt_password",'s',true,true},
 {"mqtt.topic","cloud","mqtt_base_topic",'s',true,false},
 {"websocket.enabled","cloud","websocket_enabled",'b',true,false},{"websocket.tls","cloud","websocket_tls",'b',true,false},
 {"websocket.host","cloud","websocket_host",'s',true,false},{"websocket.port","cloud","websocket_port",'n',true,false},
 {"websocket.path","cloud","websocket_path",'s',true,false},{"websocket.token","cloud","websocket_token",'s',true,true},
 {"telemetry.seconds","cloud","telemetry_seconds",'n',true,false},
 {"shell.enabled","shell","enabled",'b',true,false},{"shell.port","shell","port",'n',true,false},
 {"shell.color","shell","color",'b',true,false}
};
const ShellProperty *shellProperty(const String &key){for(const auto &p:shellProperties)if(key==p.key)return &p;return nullptr;}
String shellGet(const ShellProperty &p){
  const String key(p.key);
  if(p.secret)return F("<write-only>");
  if(key=="hostname")return deviceIdentity.hostname();if(key=="label")return deviceIdentity.displayName();
  if(key=="version")return BoardCore::BuildInfo::kVersion;if(key=="boot_id")return pcBootId;
  if(key=="uptime")return String(millis()/1000U);if(key=="heap")return String(ESP.getFreeHeap());
  if(key=="avr.online")return avrOnline?F("true"):F("false");
  if(key=="display.ack")return tm1637DetectionKnown()?(tm1637Ack?F("true"):F("false")):F("unknown");
  if(key=="firmware.phase")return PakarUpdate::name(otaProgress.snapshot().phase);
  if(key=="network.mode")return networkConfig.accessPointEnabled()?(networkConfig.stationEnabled()?F("both"):F("ap")):F("station");
  if(key=="network.ssid")return networkConfig.stationSsid();
  if(key=="led.enabled")return grid.outputEnabled()?F("true"):F("false");
  if(key=="led.mode")return grid.staticMode()?F("static"):F("effects");
  if(key=="led.brightness")return String(grid.brightness());if(key=="led.effect")return grid.patternName();
  if(key=="shell.enabled")return shellSettings.enabled()?F("true"):F("false");
  if(key=="shell.port")return String(shellSettings.port());if(key=="shell.color")return shellSettings.color()?F("true"):F("false");
  JsonDocument d;const String json=String(p.domain)=="driver"?grid.outputCapabilitiesJson():String(p.domain)=="network"?networkConfigurationJson():cloudConfigurationJson();
  if(deserializeJson(d,json))return F("unknown");
  const auto v=d[p.field];if(v.is<bool>())return v.as<bool>()?F("true"):F("false");
  if(v.is<const char*>())return v.as<String>();if(v.is<uint32_t>())return String(v.as<uint32_t>());return F("unknown");
}
bool shellBusy(){return urbootBusy.get()||remoteFirmware.busy()||otaActive||otaRebootAt||avrUploadOwner||programmingLatched||cloudServices.ioActive();}
bool shellSet(const String &key,const String &value,String &result){
  const auto *p=shellProperty(key);if(!p||!p->writable){result=F("Unknown or read-only property");return false;}
  if(shellBusy()){result=F("Maintenance active; nothing changed");return false;}
  uint32_t number=0;bool boolean=false;
  if(p->type=='b'){
    if(value!="true"&&value!="false"&&value!="on"&&value!="off"&&value!="1"&&value!="0"){result=F("Use true or false");return false;}
    boolean=value=="true"||value=="on"||value=="1";
  }
  if(p->type=='n'&&!PakarShell::unsignedNumber(value.c_str(),65535,number)){result=F("Invalid bounded integer");return false;}
  if(String(p->domain)=="shell"){
    JsonDocument patch;if(p->type=='b')patch[p->field]=boolean;else patch[p->field]=number;
    return updateShellConfiguration(patch.as<JsonObjectConst>(),result);
  }
  JsonDocument d;
  if(String(p->domain)=="device"){
    d["command"]="device-identity";d["hostname"]=key=="hostname"?value:String(deviceIdentity.hostname());
    d["display_name"]=key=="label"?value:String(deviceIdentity.displayName());
  }else if(String(p->domain)=="led"){
    if(key=="led.brightness"){d["command"]="brightness";d["value"]=number;}
    else if(key=="led.effect"){d["command"]="effect";d["value"]=value;}
    else {d["command"]="led-output";if(p->type=='b')d[p->field]=boolean;else d[p->field]=value;}
  }else{
    d["command"]=String(p->domain)=="network"?"configure-network":"configure-cloud";
    auto settings=d["settings"].to<JsonObject>();
    if(key=="network.mode"){
      if(value!="ap"&&value!="station"&&value!="both"){result=F("Use ap, station, or both");return false;}
      settings["ap_enabled"]=value!="station";settings["station_enabled"]=value!="ap";
    }else if(p->type=='b')settings[p->field]=boolean;
    else if(p->type=='n')settings[p->field]=number;else settings[p->field]=value;
  }
  String json;serializeJson(d,json);return handleCloudCommand(json,result);
}
void shellPrompt(DeviceShellSession &s){
  s.editor.setPrompt(deviceIdentity.hostname(),s.color);
  s.editor.prompt([&](const uint8_t *p,size_t n){s.write(p,n);});
}
void shellSafePrint(DeviceShellSession &s,const String &message){
  char safe[513];for(size_t at=0;at<message.length();at+=512){
    PakarShell::terminalText(safe,sizeof(safe),message.c_str()+at,min<size_t>(512,message.length()-at));s.print(safe);
  }
}
void shellResult(DeviceShellSession &s,bool accepted,const String &message){
  if(message.length()+s.output.length()+128U>8192U){
    s.println(F("ERR Response exceeds terminal window; use the JSON API for the full snapshot"));return;
  }
  if(s.color)s.print(accepted?F("\x1b[32m"):F("\x1b[31m"));
  s.print(accepted?F("OK "):F("ERR "));shellSafePrint(s,message);
  if(s.color)s.print(F("\x1b[0m"));s.println();
}
void shellTuiDraw(DeviceShellSession &s,bool force=false);
void shellTuiInput(DeviceShellSession &s,uint8_t ch,bool flushEscape=false);
String shellWifiScanJson(){
  JsonDocument d;const int count=WiFi.scanComplete();d["running"]=count==-1;d["complete"]=count>=0;
  auto rows=d["networks"].to<JsonArray>();for(int i=0;i<count&&i<40;++i){auto row=rows.add<JsonObject>();row["ssid"]=WiFi.SSID(i);row["rssi"]=WiFi.RSSI(i);row["security"]=WiFi.encryptionType(i);}
  String json;serializeJson(d,json);return json;
}

void executeDeviceShell(DeviceShellSession &s,String line){
  line.trim();if(!line.length()){shellPrompt(s);return;}
  if(s.output.length()>1024U){shellResult(s,false,F("Terminal busy; drain output before another command"));shellPrompt(s);return;}
  const int space=line.indexOf(' ');String verb=space<0?line:line.substring(0,space);
  verb.toLowerCase();String args=space<0?String():line.substring(space+1);args.trim();
  const bool query=verb=="help"||verb=="?"||verb=="get"||verb=="status"||verb=="info"||verb=="properties"||verb=="history"||(verb=="firmware"&&args=="status");
  if(shellBusy()&&!query){shellResult(s,false,F("Maintenance active; command not dispatched"));shellPrompt(s);return;}
  if(verb=="exit"||verb=="quit"){s.println(F("Goodbye."));s.closing=true;return;}
  if(verb=="tui"){s.tui.begin(s.color);shellTuiDraw(s,true);return;}
  if(verb=="help"||verb=="?"){
    s.println(F("tui | properties | get KEY | set KEY VALUE | status | info | telemetry"));
    s.println(F("cell ID white|red|off [immediate|fade|pulse] | cells IDS STATE [EFFECT]"));
    s.println(F("led-output on|off|static|effects | effect none|rainbow|fire|cell-chase | brightness1..255"));
    s.println(F("pixel INDEX[0..479] [1|6] [RRGGBB] | pixel off | audio CUE | display VALUE | display stop"));
    s.println(F("network scan | network scan-results | network reconnect | firmware status|fetch|apply|cancel ..."));
    s.println(F("clear|cls | echo TEXT | hex NUMBER | upper|noupper | color on|off | watch on|off | history | exit"));
    s.println(F("Editing: arrows/Home/End/Delete, Ctrl-A/E/U/W/L/C; Up/Down recall8 commands. TUI: Tab/Arrows/Enter/Esc."));
  }else if(verb=="properties"||verb=="get"){
    if(args.length()){
      const auto *p=shellProperty(args);if(p){shellSafePrint(s,shellGet(*p));s.println();}else shellResult(s,false,F("Unknown property"));
    }else for(const auto &p:shellProperties){s.print(p.writable?F("rw "):F("ro "));s.print(p.key);s.print(F(" = "));shellSafePrint(s,shellGet(p));s.println();}
  }else if(verb=="set"){
    const int at=args.indexOf(' ');String result;
    if(at<1)shellResult(s,false,F("Use set KEY VALUE"));
    else{String value=args.substring(at+1);value.trim();if(value.length()>=2&&value[0]=='"'&&value[value.length()-1]=='"')value=value.substring(1,value.length()-1);
      shellResult(s,shellSet(args.substring(0,at),value,result),result);}
  }else if(verb=="clear"||verb=="cls")s.print(F("\x1b[H\x1b[2J"));
  else if(verb=="echo"){if(s.upper)args.toUpperCase();s.println(args);}
  else if(verb=="history")s.editor.printHistory([&](const uint8_t *p,size_t n){s.write(p,n);});
  else if(verb=="upper"||verb=="noupper")s.upper=verb=="upper";
  else if(verb=="color"||verb=="watch"){
    if(args!="on"&&args!="off")shellResult(s,false,F("Use on or off"));
    else{if(verb=="color")s.color=args=="on";else s.watch=args=="on";shellResult(s,true,F("Session setting changed"));}
  }else if(verb=="hex"){
    uint32_t number;if(!PakarShell::unsignedNumber(args.c_str(),UINT32_MAX,number))shellResult(s,false,F("Invalid unsigned integer"));
    else {s.print(F("0x"));s.println(number,HEX);}
  }else if(verb=="display"&&args.startsWith("text ")){
    String rest=args.substring(5),fields[5];bool valid=true;uint32_t values[4]{};
    for(unsigned i=0;i<5;++i){const int at=rest.indexOf(' ');if(at<1){valid=false;break;}fields[i]=rest.substring(0,at);rest=rest.substring(at+1);}
    for(unsigned i=0;i<4;++i)valid=valid&&PakarShell::unsignedNumber(fields[i+1].c_str(),i<2?65535U:i==2?255U:1U,values[i]);
    String result;const bool ok=valid&&submitDisplayText(rest,values[0],values[1],fields[0],values[3]!=0,values[2],result);
    shellResult(s,ok,result.length()?result:String("Use display text once|loop|interval SPEED HOLD PAUSE SCROLL TEXT"));
  }else if(verb=="status"){
    s.print(F("Host "));s.print(hostname);s.print(F(" | STA "));s.print(networkConfig.stationConnected()?F("up"):F("down"));
    s.print(F(" | AP "));s.print(apStarted?F("up"):F("down"));s.print(F(" | AVR "));s.println(avrOnline?F("up"):F("down"));
    s.println(grid.outputJson());
  }else if(verb=="info"||verb=="telemetry"||verb=="firmware"){
    String result;const bool ok=handleMaintenanceShell(verb+(args.length()?" "+args:String()),result);shellResult(s,ok,result);
  }else if(verb=="network"){
    const auto code=args=="scan"?espPanelSettings.scan():args=="reconnect"?espPanelSettings.reconnect():kUnsupported;
    if(args=="scan-results")s.println(shellWifiScanJson());
    else shellResult(s,code==kNoError,code==kNoError?F("Queued; use network scan-results for completed scan"):F("Unknown action or device busy"));
  }else if(verb=="device"&&args=="identity")s.println(deviceNameJson());
  else if(verb=="device"&&(args.startsWith("rename ")||args.startsWith("label "))){
    String result;const bool rename=args.startsWith("rename ");shellResult(s,shellSet(rename?"hostname":"label",args.substring(rename?7:6),result),result);
  }else{
    JsonDocument d;d["command"]=verb;
    bool valid=true;String result;
    if(verb=="led-output"){
      if(args=="on"||args=="off")d["enabled"]=args=="on";
      else if(args=="static"||args=="effects")d["mode"]=args;else valid=false;
    }else if(verb=="brightness"||verb=="display"){
      uint32_t number;if(verb=="display"&&args=="stop")d["release"]=true;
      else if(verb=="display"&&args.startsWith("-")){
        if(!PakarShell::unsignedNumber(args.substring(1).c_str(),999,number))valid=false;else d["value"]=-int32_t(number);
      }else if(!PakarShell::unsignedNumber(args.c_str(),verb=="display"?9999U:255U,number))valid=false;
      else d["value"]=number;
    }else if(verb=="mode"||verb=="effect")d["value"]=args;
    else if(verb=="audio")d["cue"]=args;
    else if(verb=="cell"||verb=="cells"){
      const int a=args.indexOf(' '),b=a<0?-1:args.indexOf(' ',a+1);
      if(a<1)valid=false;else{
        if(verb=="cell"){uint32_t id;if(!PakarShell::unsignedNumber(args.substring(0,a).c_str(),80,id)||!id)valid=false;else d["id"]=id;}
        else d["ids"]=args.substring(0,a);
        d["state"]=b<0?args.substring(a+1):args.substring(a+1,b);d["effect"]=b<0?String("immediate"):args.substring(b+1);
      }
    }else if(verb=="pixel"){
      d["command"]="pixel-diagnostic";d["off"]=args=="off";d["index"]=0;d["count"]=1;d["rgb"]=0xffffff;
      if(args!="off"){
        const int a=args.indexOf(' '),b=a<0?-1:args.indexOf(' ',a+1);uint32_t index=0,count=1;
        const String position=a<0?args:args.substring(0,a);
        valid=PakarShell::unsignedNumber(position.c_str(),479,index);
        if(a>=0)valid=valid&&PakarShell::unsignedNumber((b<0?args.substring(a+1):args.substring(a+1,b)).c_str(),6,count)&&(count==1||count==6);
        uint32_t rgb=0xffffff;if(b>=0){String hex=args.substring(b+1);valid=valid&&hex.length()==6;rgb=0;
          for(size_t i=0;i<hex.length()&&valid;++i){const char c=hex[i];const int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(digit<0)valid=false;else rgb=(rgb<<4)|digit;}}
        d["index"]=index;d["count"]=count;d["rgb"]=rgb;
      }
    }else if(verb!="reset")valid=false;
    if(!valid)shellResult(s,false,F("Invalid command/arguments; use help"));
    else{String json;serializeJson(d,json);const bool ok=handleCloudCommand(json,result);shellResult(s,ok,result);}
  }
  if(!s.closing&&!s.tui.active())shellPrompt(s);
}
void initializeDeviceShell(DeviceShellSession &s,bool telnet){
  s.accessRevision=accessConfig.securityRevision();
  s.authenticated=accessConfig.ready()&&!accessConfig.enabled();s.color=shellSettings.color();s.lastInput=s.lastDrain=millis();
  if(telnet)s.parser.greet([&](const uint8_t *p,size_t n){s.write(p,n);});
  if(s.color)s.print(F("\x1b[1;36m"));s.print(APP_NAME);s.print(F(" @ "));s.print(deviceIdentity.hostname());
  if(s.color)s.print(F("\x1b[0m"));s.println(F("  |  help / tui"));
  s.editor.setEcho(s.authenticated);if(s.authenticated)shellPrompt(s);else s.print(F("Password: "));
}
void inputDeviceShell(DeviceShellSession &s,uint8_t ch){
  if(!accessConfig.ready()||s.accessRevision!=accessConfig.securityRevision()){s.closing=true;return;}
  if(s.closing||s.fault)return;s.lastInput=millis();
  if(s.tui.active()){shellTuiInput(s,ch);s.escapeAt=millis();return;}
  const auto event=s.editor.feed(ch,[&](const uint8_t *p,size_t n){s.write(p,n);});
  if(event==PakarShell::LineEditor::None)return;
  if(event==PakarShell::LineEditor::Close){s.closing=true;return;}
  if(event==PakarShell::LineEditor::Cancel||event==PakarShell::LineEditor::TooLong){
    if(event==PakarShell::LineEditor::TooLong)shellResult(s,false,F("Line too long; discarded"));s.editor.clear();
    if(s.authenticated)shellPrompt(s);else s.print(F("Password: "));return;
  }
  if(!s.authenticated){
    const bool valid=accessConfig.ready()&&BoardCore::Access::equal(s.editor.line(),accessConfig.password());s.editor.clear();
    if(!valid){s.println(F("Authentication failed."));s.closing=true;return;}
    s.authenticated=true;s.editor.setEcho(true);shellPrompt(s);return;
  }
  String line(s.editor.line());bool historySafe=true;
  if(line.startsWith("set ")){const int at=line.indexOf(' ',4);const auto *property=at>4?shellProperty(line.substring(4,at)):nullptr;historySafe=property&&!property->secret;}
  if(line.indexOf("://")>=0)historySafe=false;
  s.editor.remember(historySafe);s.editor.clear();executeDeviceShell(s,line);
}
void closeDeviceShells(){
  // Auth callbacks can run on AsyncTCP's task. Only the main loop destroys
  // editor/socket owners; input is independently fenced by accessRevision.
  shellCloseRequested=true;
}
void retireDeviceShells(){
  for(unsigned i=0;i<PAKAR_TELNET_SESSIONS;++i){shellPeers[i].stop();telnetSessions[i].reset();}
  serialShell.reset();shellChannel.close();
}
unsigned shellConnectionCount(){unsigned n=0;for(unsigned i=0;i<PAKAR_TELNET_SESSIONS;++i)if(telnetSessions[i]&&shellPeers[i].connected())++n;return n;}
uint16_t shellAdvertisedPort(){return shellListening?shellListeningPort:0;}
String shellStatusJson(){
  JsonDocument d;d["enabled"]=bool(PAKAR_TELNET_ENABLED)&&shellSettings.enabled();d["port"]=shellSettings.port();
  d["listening"]=shellListening;d["sessions"]=shellConnectionCount();d["session_limit"]=PAKAR_TELNET_SESSIONS;
  d["color"]=shellSettings.color();d["history_limit"]=8;d["serial_encapsulated"]=bool(PAKAR_SERIAL_SHELL_ENABLED);
  d["serial_open"]=shellChannel.active();d["tui"]=true;String json;serializeJson(d,json);return json;
}
void beginDeviceShells(){shellSettings.begin(LittleFS);shellRebindAt=millis();}
void serviceDeviceShells(){
  const uint32_t now=millis();
#if defined(ARDUINO_ARCH_ESP32)
  if(shellCloseRequested.exchange(false))retireDeviceShells();
#else
  if(shellCloseRequested){shellCloseRequested=false;retireDeviceShells();}
#endif
  if(shellRebindAt&&int32_t(now-shellRebindAt)>=0){
    shellRebindAt=0;retireDeviceShells();telnetServer.stop();shellListening=false;shellListeningPort=0;
    if(PAKAR_TELNET_ENABLED&&shellSettings.enabled()&&accessConfig.ready()){
      telnetServer.begin(shellSettings.port());telnetServer.setNoDelay(true);
#if defined(ARDUINO_ARCH_ESP32)
      shellListening=bool(telnetServer);
#else
      shellListening=telnetServer.status()==LISTEN;
#endif
      if(shellListening)shellListeningPort=shellSettings.port();
    }
    startDiscovery();
  }
  if(shellListening&&telnetServer.hasClient()){
    WiFiClient candidate=telnetServer.accept();bool admitted=false;
    for(unsigned i=0;i<PAKAR_TELNET_SESSIONS;++i)if(!telnetSessions[i]){
      auto *session=new(std::nothrow) DeviceShellSession;if(!session)break;
      shellPeers[i]=candidate;shellPeers[i].setNoDelay(true);telnetSessions[i].reset(session);initializeDeviceShell(*session,true);admitted=true;break;
    }
    if(!admitted)candidate.stop();
  }
  for(unsigned i=0;i<PAKAR_TELNET_SESSIONS;++i){
    if(!telnetSessions[i])continue;auto &s=*telnetSessions[i];auto &peer=shellPeers[i];
    if(!peer.connected()||s.fault||!accessConfig.ready()||s.accessRevision!=accessConfig.securityRevision()||uint32_t(now-s.lastInput)>(s.authenticated?1800000U:60000U)||
        (s.output.length()&&uint32_t(now-s.lastDrain)>30000U)){peer.stop();telnetSessions[i].reset();continue;}
    if(s.output.length()){
      const size_t count=min<size_t>(192,s.output.length());int sent=0;
#if defined(ARDUINO_ARCH_ESP32)
      // One nonblocking write. Slow terminals cannot hold up UART/LED/API work.
      sent=::send(peer.fd(),s.output.c_str(),count,MSG_DONTWAIT);
      if(sent<0&&errno!=EWOULDBLOCK&&errno!=EAGAIN){s.fault=true;continue;}
#else
      const size_t available=peer.availableForWrite();if(available)sent=peer.write(reinterpret_cast<const uint8_t*>(s.output.c_str()),min(count,available));
#endif
      if(sent>0){s.output.remove(0,sent);s.lastDrain=now;}
    }
    if(s.closing){if(!s.output.length()){peer.stop();telnetSessions[i].reset();}continue;}
    // Backpressure before consuming more commands; no truncation or replay.
    unsigned budget=s.output.length()<1024?64:0;bool terminalInput=false;
    while(budget--&&peer.available()&&s.output.length()<1024U){
      const int byte=peer.read();if(byte<0)break;
      terminalInput=true;
      s.parser.feed(uint8_t(byte),[&](const uint8_t *p,size_t n){s.write(p,n);},[&](uint8_t ch){inputDeviceShell(s,ch);});
      if(s.closing||s.fault)break;
    }
    if(s.tui.active()&&s.tui.escapePending()&&uint32_t(millis()-s.escapeAt)>60){shellTuiInput(s,0,true);terminalInput=true;}
    if(terminalInput&&s.tui.active()&&s.output.length()<4096U)shellTuiDraw(s);
  }
  if(serialShell&&(!accessConfig.ready()||serialShell->accessRevision!=accessConfig.securityRevision()||uint32_t(now-serialShell->lastInput)>30000U)){serialShell.reset();shellChannel.close();}
  if(uint32_t(now-lastShellObserve)>=500U){
    lastShellObserve=now;
    String observed=String("event STA ")+(networkConfig.stationConnected()?"up":"down")+" AP "+(apStarted?"up":"down")+" AVR "+(avrOnline?"up":"down")+" update "+(remoteFirmware.busy()?String(remoteFirmware.phase()):String(PakarUpdate::name(otaProgress.snapshot().phase)));
    const auto progress=remoteFirmware.busy()?remoteFirmware.progress():otaProgress.snapshot();
    if(progress.known())observed+=" "+String(progress.percent())+"%";
    unsigned red=0,off=0;for(uint8_t id=1;id<=80;++id){red+=grid.state(id)==kCellRed;off+=grid.state(id)==kCellOff;}
    observed+=" | cells red "+String(red)+" off "+String(off)+" | output "+String(grid.outputEnabled()?"on":"off");
    if(observed!=shellObserved){shellObserved=observed;
      for(auto &slot:telnetSessions)if(slot&&slot->authenticated&&slot->watch&&!slot->tui.active()&&slot->output.length()<1024U)
        slot->editor.redrawEvent(observed.c_str(),[&](const uint8_t *p,size_t n){slot->write(p,n);});
      if(serialShell&&serialShell->authenticated&&serialShell->watch&&!serialShell->tui.active()&&serialShell->output.length()<1024U)
        serialShell->editor.redrawEvent(observed.c_str(),[&](const uint8_t *p,size_t n){serialShell->write(p,n);});
    }
    for(auto &slot:telnetSessions)if(slot&&slot->authenticated&&slot->tui.active()&&slot->output.length()<1024U)shellTuiDraw(*slot);
    if(serialShell&&serialShell->authenticated&&serialShell->tui.active()&&serialShell->output.length()<1024U)shellTuiDraw(*serialShell);
  }
}
bool updateShellConfiguration(JsonObjectConst patch,String &result){
  if(shellBusy()||networkApplyPending||cloudApplyPending){result=F("Device busy; shell settings unchanged");return false;}
  if(patch.isNull()||!patch.size()||patch.size()>3){result=F("Supply enabled, port and/or color");return false;}
  bool enabled=shellSettings.enabled(),color=shellSettings.color();uint16_t port=shellSettings.port();
  for(JsonPairConst pair:patch){const String key=pair.key().c_str();const auto value=pair.value();
    if(key=="enabled"||key=="color"){
      if(!value.is<bool>()){result=F("Shell enabled/color must be boolean");return false;}
      if(key=="enabled")enabled=value.as<bool>();else color=value.as<bool>();
    }else if(key=="port"){
      if(!value.is<uint16_t>()||!value.as<uint16_t>()){result=F("Invalid shell TCP port");return false;}port=value.as<uint16_t>();
    }else{result=F("Unknown shell setting");return false;}
  }
  if(!shellSettings.configure(enabled,port,color)){result=F("Invalid/reserved port or durable shell save failed");return false;}
  shellRebindAt=millis()+1000U;result=F("Shell settings saved; listener applies after response");return true;
}
void registerShellApi(){
  server.on("/api/shell",HTTP_GET,[](){if(authenticate())server.send(200,"application/json",shellStatusJson());});
  server.on("/api/shell",HTTP_POST,[](){
    if(!authenticate())return;
    JsonDocument patch;
    for(const char *key:{"enabled","color"})if(server.hasArg(key)){
      const String value=server.arg(key);if(value!="true"&&value!="false"){server.send(400,"text/plain","Use true or false");return;}patch[key]=value=="true";
    }
    if(server.hasArg("port")){uint32_t number;if(!PakarShell::unsignedNumber(server.arg("port").c_str(),65535,number)||!number){server.send(400,"text/plain","Invalid port");return;}patch["port"]=number;}
    String result;const bool ok=updateShellConfiguration(patch.as<JsonObjectConst>(),result);server.send(ok?200:409,"application/json",String("{\"accepted\":")+(ok?"true":"false")+",\"shell\":"+shellStatusJson()+"}");
  });
}

bool handleShellFrame(const BoardCore::Protocol::Frame &frame){
  if(frame.opcode!=kShellRequest)return false;
#if !PAKAR_SERIAL_SHELL_ENABLED
  hostLink.error(frame.sequence,frame.opcode,kUnsupported);return true;
#else
  const auto *p=frame.payload;const uint8_t n=frame.payloadLength;
  if(!n){hostLink.error(frame.sequence,frame.opcode,kBadPayload);return true;}
  if(p[0]==PakarShell::SerialChannel::Open){
    uint8_t boot[16];if(n!=21||pcBootId.length()!=32){hostLink.error(frame.sequence,frame.opcode,kBadPayload);return true;}
    for(unsigned i=0;i<16;++i){char hex[3]={pcBootId[i*2],pcBootId[i*2+1],0};boot[i]=strtoul(hex,nullptr,16);}
    if(memcmp(p+1,boot,16)){hostLink.error(frame.sequence,frame.opcode,kUnsafe);return true;}
    if(serialShell){hostLink.error(frame.sequence,frame.opcode,kBusy);return true;}
    if(shellTokenCounter==UINT32_MAX){hostLink.error(frame.sequence,frame.opcode,kUnsafe);return true;}
    auto *session=new(std::nothrow) DeviceShellSession;if(!session){hostLink.error(frame.sequence,frame.opcode,kBusy);return true;}
    serialShell.reset(session);++shellTokenCounter;shellChannel.open(shellTokenCounter);initializeDeviceShell(*session,false);
    uint8_t response[13]={0};memcpy(response+1,&shellTokenCounter,4);memcpy(response+9,p+17,4);
    hostLink.send(kShellResponse,frame.sequence,response,sizeof(response));return true;
  }
  if(n<5){hostLink.error(frame.sequence,frame.opcode,kBadPayload);return true;}
  uint32_t token;memcpy(&token,p+1,4);
  if(!serialShell||token!=shellChannel.token()||!accessConfig.ready()||serialShell->accessRevision!=accessConfig.securityRevision()){hostLink.error(frame.sequence,frame.opcode,kUnsafe);return true;}
  auto &s=*serialShell;s.lastInput=millis();
  if(p[0]==PakarShell::SerialChannel::Input){
    if(n<8){hostLink.error(frame.sequence,frame.opcode,kBadPayload);return true;}
    const uint16_t seq=p[5]|uint16_t(p[6])<<8;
    if(s.output.length()>1024U){hostLink.error(frame.sequence,frame.opcode,kBusy);return true;}
    const auto admission=shellChannel.accept(token,seq,p+7,n-7);
    if(admission!=PakarShell::SerialChannel::Accepted&&admission!=PakarShell::SerialChannel::Duplicate){hostLink.error(frame.sequence,frame.opcode,kUnsafe);return true;}
    if(admission==PakarShell::SerialChannel::Accepted)for(unsigned i=7;i<n;++i)inputDeviceShell(s,p[i]);
    if(s.tui.active()&&s.output.length()<4096U)shellTuiDraw(s);
    hostLink.ack(frame.sequence,frame.opcode);return true;
  }
  if(p[0]==PakarShell::SerialChannel::Close&&n==5){serialShell.reset();shellChannel.close();hostLink.ack(frame.sequence,frame.opcode);return true;}
  if(p[0]!=PakarShell::SerialChannel::Poll||n!=7){hostLink.error(frame.sequence,frame.opcode,kBadPayload);return true;}
  const uint16_t ack=p[5]|uint16_t(p[6])<<8;size_t consumed=0;
  if(!s.serialOutput.acknowledge(ack,consumed)){hostLink.error(frame.sequence,frame.opcode,kUnsafe);return true;}
  if(consumed)s.output.remove(0,consumed);
  if(s.tui.active()&&s.tui.escapePending()&&uint32_t(millis()-s.escapeAt)>60)shellTuiInput(s,0,true);
  uint8_t response[48]={3};memcpy(response+1,&token,4);const uint16_t next=shellChannel.next();memcpy(response+5,&next,2);
  if(!s.serialOutput.stage(reinterpret_cast<const uint8_t*>(s.output.c_str()),s.output.length())){s.fault=true;s.closing=true;}
  response[7]=uint8_t(s.closing)|(s.fault?2:0);
  const uint16_t outSequence=s.serialOutput.sequence();memcpy(response+8,&outSequence,2);
  const uint8_t count=s.serialOutput.size();memcpy(response+10,s.serialOutput.bytes(),count);
  hostLink.send(kShellResponse,frame.sequence,response,count+10); // Only a later explicit ACK retires bytes.
  return true;
#endif
}
