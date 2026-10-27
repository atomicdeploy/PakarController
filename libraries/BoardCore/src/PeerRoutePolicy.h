#pragma once
#include <stdint.h>
#include <string.h>
#include <initializer_list>

namespace BoardCore { namespace PeerRoutePolicy {
// Adapted from PCController netpolicy/http_url.go. This MCU relay deliberately
// supports only explicit LAN hostnames, no arbitrary URL, credentials or port.
inline bool hostname(const char *s) {
  if(!s || !*s || strlen(s)>253 || !strcmp(s,"localhost"))return false;
  unsigned label=0;bool alpha=false;char previous=0;
  for(;*s;++s) {
    const char c=*s;
    if(c=='.') {if(!label||previous=='-')return false;label=0;}
    else {
      if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-')||(!label&&c=='-')||++label>63)return false;
      alpha|=c>='a'&&c<='z';
    }
    previous=c;
  }
  return label&&previous!='-'&&alpha;
}
inline bool privateAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d) {
  (void)c;
  return d!=0&&d!=255&&(a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168));
}
inline bool hexId(const char *s) {
  if(!s||strlen(s)!=32)return false;
  bool nonzero=false;
  for(unsigned i=0;i<32;++i){if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;nonzero|=s[i]!='0';}
  return nonzero;
}
inline bool boardId(const char *s) {
  if(!s||!*s||strlen(s)>64)return false;
  for(;*s;++s)if(!((*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'))return false;
  return true;
}
inline bool readPath(const char *s) {
  if(!s)return false;
  for(const char *p:{"/api/peer/info","/api/status","/api/panel","/api/cells/mapping","/api/table/snapshot","/api/update/lifecycle"})if(!strcmp(s,p))return true;
  return false; // No recursive relays, raw files, firmware or secrets.
}
} }
