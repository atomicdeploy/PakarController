#include "../firmware/esp/src/RemotePolicy.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <initializer_list>
int main() {
  using namespace RemotePolicy;
  assert(url("https://firmware.digitalogic.ir/PakarController/manifest.json"));
  assert(url("https://ledboard.local:8443/fw.bin?key=value"));
  assert(url("http://localhost:8118/file",true));
  for(const char *s:{"http://host/file","https://a@host/file","https://host:0/file",
      "https://host:65536/file","https://host:1:2/file","https://host/file#frag",
      "https://host\\evil/file","https://%68ost/file","https://-host/file",
      "https://host./file","https://host?evil/file","https://host/ file","https://host"})
    assert(!url(s));
  uint16_t parts[3];
  assert(version("0.2.7",parts) && parts[2]==7);
  for(const char *s:{"1.2","1.2.3.4","1.2.256","1..3","1.2.-1","1.2.3x"}) assert(!version(s,parts));
  assert(compare("0.2.7","0.2.6")==1 && compare("0.2.5","0.2.6")==-1);
  Transfer t{32768,0}; assert(t.append(32767)); assert(!t.complete(true));
  assert(!t.append(2)); assert(t.append(1)); assert(!t.complete(false)); assert(t.complete(true));
  assert(!t.append(SIZE_MAX));
  uint8_t esp[]={0xE9,1,0,0,0,0,0,0},avr[]={0x0c,0x94,0,0};
  assert(espHeader(esp,8)&&avrVector(avr,4)); esp[1]=0; assert(!espHeader(esp,8));
  avr[0]=0;avr[1]=0;assert(!avrVector(avr,4));
  uint32_t epoch=1;
  assert(utc("1970-01-01T00:00:00Z",epoch)&&epoch==0);
  assert(utc("2000-02-29T12:00:00Z",epoch)&&epoch==951825600);
  for(const char *s:{"2001-02-29T12:00:00Z","2026-13-01T00:00:00Z",
      "2026-01-01T24:00:00Z","2026-01-01T00:00:60Z","2026-02-30T00:00:00Z",
      "2026-01-01T00:00:00","2026-01-01T00:00:00X"}) assert(!utc(s,epoch));
  puts("Remote policy: URL/identity bounds, target headers, exact size/digest and strict UTC passed.");
}
