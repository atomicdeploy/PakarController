#include "../firmware/esp8266/src/ShellProperties.h"
#include <cassert>
#include <cstdio>
int main(){
 constexpr unsigned added=
#if defined(ARDUINO_ARCH_ESP32)
 4;
#else
 0;
#endif
 assert(shellPropertyCount==44+added);
 unsigned secrets=0,writable=0;
 for(size_t index=0;index<shellPropertyCount;++index){
  const auto property=shellPropertyAt(index);ShellProperty found{};
  assert(property.key&&property.domain&&property.field);
  assert(shellProperty(property.key,found));
  assert(!strcmp(property.key,found.key)&&!strcmp(property.domain,found.domain)&&!strcmp(property.field,found.field));
  assert(property.type==found.type&&property.writable==found.writable&&property.secret==found.secret);
  for(size_t earlier=0;earlier<index;++earlier)assert(strcmp(property.key,shellPropertyAt(earlier).key));
  secrets+=property.secret;writable+=property.writable;
 }
 assert(secrets==3+added/2&&writable==32+added);
 ShellProperty ntfy{};
#if defined(ARDUINO_ARCH_ESP32)
 assert(shellProperty("ntfy.enabled",ntfy)&&ntfy.writable&&!ntfy.secret);
 assert(shellProperty("ntfy.topic",ntfy)&&ntfy.secret);
 assert(shellProperty("ntfy.token",ntfy)&&ntfy.secret);
#else
 assert(!shellProperty("ntfy.enabled",ntfy));
#endif
 ShellProperty hostname{},password{},other{};
 assert(shellProperty("hostname",hostname)&&hostname.writable&&!hostname.secret);
 assert(shellProperty("network.password",password)&&password.secret&&password.writable);
 assert(shellProperty("websocket.token",other)&&other.secret);
 assert(!strcmp(hostname.key,"hostname")&&!strcmp(password.key,"network.password")); // Owned copies survive another lookup.
 assert(!shellProperty("missing",other)&&!other.key&&!other.secret);
 assert(!shellProperty(nullptr,other)&&!other.key);
 assert(!shellPropertyAt(shellPropertyCount).key&&!shellPropertyAt(static_cast<size_t>(-1)).key);
#if defined(ARDUINO_ARCH_ESP8266)
 assert(pakarPropertyFlashReads>shellPropertyCount*2);
 puts("All shell descriptors read through copied PROGMEM records; secret/read-only flags and lookup ownership retained.");
#else
 puts("All shell descriptors retain ordinary-memory S3/host semantics; ntfy is S3-only and topic/token are private.");
#endif
}
