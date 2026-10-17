// Select the REAL SDK ArduinoStringWriter, which clears its destination.
// The generic host write() adapter appends and hid this production regression.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "../firmware/esp8266/src/ClientTelemetry.h"
#include <cassert>
#include <cstdio>
int main(){
  JsonDocument probe;probe["probe"]=true;String replaced="must not survive";
  assert(serializeJson(probe,replaced)==measureJson(probe));
  assert(replaced=="{\"probe\":true}");
  ClientTelemetry registry;
  assert(registry.json(0)=="[]");
  assert(registry.report("{\"id\":\"one\",\"hostname\":\"ASUS\",\"application\":\"PakarController\",\"cpu_count\":8,\"password\":\"not-recorded\"}","192.0.2.1",1));
  assert(!registry.report("{\"id\":\"one\"}","192.0.2.2",2));
  assert(!registry.report("{\"id\":\"bad\",\"cpu_count\":\"8\"}","192.0.2.1",3));
  assert(!registry.report("{\"id\":\"bad\",\"os\":\"bad\\nvalue\"}","192.0.2.1",3));
  assert(!registry.report("{\"schema\":1}","192.0.2.1",3));
  JsonDocument d;registry.append(d["clients"].to<JsonArray>(),100);
  assert(d["clients"][0]["observed_ip"]=="192.0.2.1");
  assert(d["clients"][0]["cpu_count"]==8 && d["clients"][0]["memory_mb"].isNull());
  assert(d["clients"][0]["password"].isNull() && d["clients"][0]["self_reported"]==true);
  JsonDocument streamed;assert(!deserializeJson(streamed,registry.json(100)));
  assert(streamed[0]["id"]=="one" && streamed[0]["cpu_count"]==8);
  assert(streamed[0]["password"].isNull());
  pakarTestStringConcatAllowed=false;assert(registry.json(100).isEmpty());pakarTestStringConcatAllowed=true;
  assert(registry.report("{\"id\":\"one\"}","192.0.2.2",120002));
  for(unsigned i=0;i<7;++i){char b[64];snprintf(b,sizeof(b),"{\"id\":\"client%u\"}",i);assert(registry.report(b,"192.0.2.3",120003));}
  JsonDocument eight;assert(!deserializeJson(eight,registry.json(120004)));
  assert(eight.is<JsonArray>() && eight.size()==8 && eight[0]["id"]=="one" && eight[7]["id"]=="client6");
  assert(!registry.report("{\"id\":\"ninth\"}","192.0.2.4",120004));
  assert(registry.report("{\"id\":\"ninth\"}","192.0.2.4",240005));
  puts("Actual ArduinoJson ArduinoStringWriter: replaces destination; registry explicit append preserves empty/one/eight-client JSON, detects failed entry serialization, observed/self-reported distinction, secret allowlist, malformed types, capacity, peer-ID collision and TTL passed.");
}
