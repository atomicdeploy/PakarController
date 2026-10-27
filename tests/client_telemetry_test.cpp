// Select the REAL SDK ArduinoStringWriter, which clears its destination.
// The generic host write() adapter appends and hid this production regression.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "../firmware/esp/src/ClientTelemetry.h"
#include <cassert>
#include <cstdio>
int main(){
  static_assert(sizeof(ClientTelemetry)<=1440*ClientTelemetry::kCapacity,
                "Client telemetry inline RAM budget regressed");
  JsonDocument probe;probe["probe"]=true;String replaced="must not survive";
  assert(serializeJson(probe,replaced)==measureJson(probe));
  assert(replaced=="{\"probe\":true}");
  ClientTelemetry registry;
  assert(registry.json(0)=="[]");
  assert(registry.report("{\"id\":\"one\",\"version\":\"0.2.8\",\"hostname\":\"ASUS\",\"application\":\"PakarController\",\"cpu_count\":8,\"password\":\"not-recorded\"}","192.0.2.1",1));
  assert(!registry.report("{\"id\":\"one\",\"version\":\"0.2.8\"}","192.0.2.2",2));
  assert(!registry.report("{\"id\":\"bad\",\"version\":\"0.2.8\",\"cpu_count\":\"8\"}","192.0.2.1",3));
  assert(!registry.report("{\"id\":\"bad\",\"version\":\"0.2.8\",\"os\":\"bad\\nvalue\"}","192.0.2.1",3));
  assert(!registry.report("{\"schema\":1}","192.0.2.1",3));
  JsonDocument d;registry.append(d["clients"].to<JsonArray>(),100);
  assert(d["clients"][0]["observed_ip"]=="192.0.2.1");
  assert(d["clients"][0]["cpu_count"]==8 && d["clients"][0]["memory_mb"].isNull());
  assert(d["clients"][0]["password"].isNull() && d["clients"][0]["self_reported"]==true);
  JsonDocument streamed;assert(!deserializeJson(streamed,registry.json(100)));
  assert(streamed[0]["id"]=="one" && streamed[0]["cpu_count"]==8);
  assert(streamed[0]["password"].isNull());
  pakarTestStringConcatAllowed=false;assert(registry.json(100).isEmpty());pakarTestStringConcatAllowed=true;
  assert(!registry.report("{\"id\":\"old\",\"version\":\"0.2.6\"}","192.0.2.1",2));
  assert(!registry.report("{\"id\":\"missing-version\"}","192.0.2.1",2));
  assert(registry.report("{\"id\":\"one\",\"version\":\"0.2.8\"}","192.0.2.2",120002));
  for(unsigned i=1;i<ClientTelemetry::kCapacity;++i){char b[96];snprintf(b,sizeof(b),"{\"id\":\"client%u\",\"version\":\"0.2.8\"}",i);assert(registry.report(b,"192.0.2.3",120003));}
  JsonDocument full;assert(!deserializeJson(full,registry.json(120004)));
  assert(full.is<JsonArray>() && full.size()==ClientTelemetry::kCapacity && full[0]["id"]=="one");
  if(ClientTelemetry::kCapacity>1){char last[32];snprintf(last,sizeof(last),"client%u",static_cast<unsigned>(ClientTelemetry::kCapacity-1));assert(full[ClientTelemetry::kCapacity-1]["id"]==last);}
  assert(!registry.report("{\"id\":\"ninth\",\"version\":\"0.2.8\"}","192.0.2.4",120004));
  JsonDocument afterRejected;assert(!deserializeJson(afterRejected,registry.json(120004)));
  assert(afterRejected==full); // Capacity rejection never evicts an active peer.
  assert(registry.report("{\"id\":\"one\",\"version\":\"0.2.8\",\"cpu_count\":16}","192.0.2.2",120005));
  JsonDocument refreshed;assert(!deserializeJson(refreshed,registry.json(120005)));
  assert(refreshed.size()==ClientTelemetry::kCapacity && refreshed[0]["id"]=="one" && refreshed[0]["cpu_count"]==16);
  assert(registry.report("{\"id\":\"ninth\",\"version\":\"0.2.8\"}","192.0.2.4",240005));
  ClientTelemetry details;
  const String report="{\"id\":\"session\",\"session_id\":\"same-session\",\"application\":\"PakarController\",\"version\":\"0.2.8\",\"build\":14,\"hardware\":{\"model\":\"Actual model\",\"password\":\"secret\"},\"capabilities\":[\"websocket\",\"websocket\"],\"settings\":{\"theme\":\"dark\",\"token\":\"secret\"}}";
  assert(details.report(report,"192.0.2.10",1,"PakarController/0.2.8","0.2.8"));
  assert(details.report(report,"usb",2)); // Same declared session, different observed transport.
  assert(!details.report(report,"usb",3,String(),"0.2.9"));
  JsonDocument detail;assert(!deserializeJson(detail,details.json(4)));
  assert(detail[0]["observed_ip"].isNull()&&detail[0]["observed_transport"]=="usb");
  assert(detail[0]["hardware"]["model"]=="Actual model");
  assert(detail[0]["hardware"]["password"].isNull()&&detail[0]["settings"]["token"].isNull());
  assert(detail[0]["capabilities"].size()==1&&detail[0]["build"]==14);
  ClientTelemetry unicode;
  assert(unicode.report("{\"id\":\"unicode\",\"version\":\"0.2.8\",\"device_model\":\"دستگاه\"}","192.0.2.11",5));
  printf("Actual ArduinoJson ArduinoStringWriter: registry preserves empty/one/%u-client JSON, observed/self-reported distinction, secret allowlist, malformed types, full-capacity rejection without eviction, peer-ID collision and TTL passed.\n",static_cast<unsigned>(ClientTelemetry::kCapacity));
}
