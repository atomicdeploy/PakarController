#include "../firmware/esp8266/src/ClientTelemetry.h"
#include <cassert>
#include <cstdio>
int main(){
  ClientTelemetry registry;
  assert(registry.report("{\"schema\":1,\"id\":\"one\",\"hostname\":\"ASUS\",\"application\":\"PakarController\",\"cpu_count\":8,\"password\":\"not-recorded\"}","192.0.2.1",1));
  assert(!registry.report("{\"schema\":1,\"id\":\"one\"}","192.0.2.2",2));
  assert(!registry.report("{\"schema\":1,\"id\":\"bad\",\"cpu_count\":\"8\"}","192.0.2.1",3));
  assert(!registry.report("{\"schema\":1,\"id\":\"bad\",\"os\":\"bad\\nvalue\"}","192.0.2.1",3));
  assert(!registry.report("{\"schema\":1}","192.0.2.1",3));
  JsonDocument d;registry.append(d["clients"].to<JsonArray>(),100);
  assert(d["clients"][0]["observed_ip"]=="192.0.2.1");
  assert(d["clients"][0]["cpu_count"]==8 && d["clients"][0]["memory_mb"].isNull());
  assert(d["clients"][0]["password"].isNull() && d["clients"][0]["self_reported"]==true);
  JsonDocument streamed;assert(!deserializeJson(streamed,registry.json(100)));
  assert(streamed[0]["id"]=="one" && streamed[0]["cpu_count"]==8);
  assert(streamed[0]["password"].isNull());
  assert(registry.report("{\"schema\":1,\"id\":\"one\"}","192.0.2.2",120002));
  for(unsigned i=0;i<7;++i){char b[64];snprintf(b,sizeof(b),"{\"schema\":1,\"id\":\"client%u\"}",i);assert(registry.report(b,"192.0.2.3",120003));}
  assert(!registry.report("{\"schema\":1,\"id\":\"ninth\"}","192.0.2.4",120004));
  assert(registry.report("{\"schema\":1,\"id\":\"ninth\"}","192.0.2.4",240005));
  puts("Client registry: observed/self-reported distinction, secret allowlist, malformed types, capacity, peer-ID collision and TTL passed.");
}
