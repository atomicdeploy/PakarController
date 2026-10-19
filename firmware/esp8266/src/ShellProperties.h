#pragma once
#include <stddef.h>
#include <string.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <pgmspace.h>
#define PAKAR_SHELL_METADATA_STORAGE PROGMEM
#else
#define PAKAR_SHELL_METADATA_STORAGE
#endif

struct ShellProperty {const char *key,*domain,*field;char type;bool writable,secret;};
// Immutable descriptors belong in flash on the RAM-limited ESP8266. String
// pointers retain their existing lifetime/encoding; copy the descriptor before
// accessing it, rather than returning an unaligned flash-backed struct pointer.
static const ShellProperty shellProperties[] PAKAR_SHELL_METADATA_STORAGE={
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
#if defined(ARDUINO_ARCH_ESP32)
 {"ntfy.enabled","cloud","ntfy_enabled",'b',true,false},{"ntfy.host","cloud","ntfy_host",'s',true,false},
 {"ntfy.topic","cloud","ntfy_topic",'s',true,true},{"ntfy.token","cloud","ntfy_token",'s',true,true},
#endif
 {"shell.enabled","shell","enabled",'b',true,false},{"shell.port","shell","port",'n',true,false},
 {"shell.color","shell","color",'b',true,false}
};
constexpr size_t shellPropertyCount=sizeof(shellProperties)/sizeof(*shellProperties);
inline ShellProperty shellPropertyAt(size_t index){
  ShellProperty result{};if(index>=shellPropertyCount)return result;
#if defined(ARDUINO_ARCH_ESP8266)
  memcpy_P(&result,shellProperties+index,sizeof(result));
#else
  result=shellProperties[index];
#endif
  return result;
}
inline bool shellProperty(const char *key,ShellProperty &result){
  if(key)for(size_t index=0;index<shellPropertyCount;++index){
    const auto candidate=shellPropertyAt(index);
    if(!strcmp(key,candidate.key)){result=candidate;return true;}
  }
  result={};return false;
}
#undef PAKAR_SHELL_METADATA_STORAGE
