#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace RemotePolicy {
enum Target : uint8_t { Esp = 1, Avr = 2, All = 3 };
inline bool hash(const char *s) {
  if (!s || strlen(s) != 64) return false;
  for (size_t i=0;i<64;++i)
    if (!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f'))) return false;
  return true;
}
inline bool host(const char *s, size_t n) {
  if (!s || !n || n>253) return false;
  size_t label=0;
  for (size_t i=0;i<n;++i) {
    const char c=s[i];
    if (c=='.') { if (!label || label>63 || s[i-1]=='-') return false; label=0; }
    else {
      if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')) return false;
      if (!label && c=='-') return false;
      ++label;
    }
  }
  return label && label<=63 && s[n-1]!='-';
}
// HTTPS, bounded DNS/IPv4 authority; no userinfo, fragments, whitespace,
// encoded authority tricks or ambiguous ports. LAN hostnames are supported.
inline bool url(const char *s, bool allowHttp=false) {
  if (!s || strlen(s)>383) return false;
  const size_t prefix=!strncmp(s,"https://",8)?8:
      allowHttp && !strncmp(s,"http://",7)?7:0;
  if (!prefix) return false;
  for (const char *p=s;*p;++p)
    if (static_cast<unsigned char>(*p)<=32 || static_cast<unsigned char>(*p)>=127 ||
        *p=='\\' || *p=='#') return false;
  const char *end=strchr(s+prefix,'/');
  if (!end) return false;
  const char *port=nullptr;
  for (const char *p=s+prefix;p<end;++p) {
    if (*p==':') { if (port) return false; port=p; }
    if (*p=='@' || *p=='%' || *p=='?') return false;
  }
  if (!host(s+prefix,(port?port:end)-(s+prefix))) return false;
  if (port) {
    uint32_t value=0;
    if (port+1==end || end-port>6) return false;
    for (const char *p=port+1;p<end;++p) {
      if (*p<'0'||*p>'9') return false;
      value=value*10+*p-'0';
    }
    if (!value || value>65535) return false;
  }
  return true;
}
inline bool version(const char *s, uint16_t out[3]) {
  if (!s || strlen(s)>15) return false;
  for (uint8_t i=0;i<3;++i) {
    if (*s<'0'||*s>'9') return false;
    uint32_t n=0; unsigned digits=0;
    do { n=n*10+*s++-'0'; if (++digits>3 || n>255) return false; } while (*s>='0'&&*s<='9');
    out[i]=n;
    if (i<2) { if (*s++!='.') return false; } else if (*s) return false;
  }
  return true;
}
inline int compare(const char *a, const char *b) {
  uint16_t av[3],bv[3];
  if (!version(a,av)||!version(b,bv)) return -2;
  for (uint8_t i=0;i<3;++i) if (av[i]!=bv[i]) return av[i]>bv[i]?1:-1;
  return 0;
}
struct Transfer {
  uint32_t expected=0, received=0;
  bool append(size_t n) {
    if (!expected || received>expected || n>expected-received) return false;
    received+=n; return true;
  }
  bool complete(bool digestMatches) const {
    return expected && received==expected && digestMatches;
  }
};
inline bool espHeader(const uint8_t *p, size_t n) {
  return p && n>=8 && p[0]==0xE9 && p[1]>=1 && p[1]<=16;
}
inline bool avrVector(const uint8_t *p, size_t n) {
  if (!p || n<4) return false;
  const uint16_t word=p[0]|static_cast<uint16_t>(p[1])<<8;
  return word==0x940C || (word&0xF000)==0xC000;
}
// Strict Gregorian UTC, without PU calendar/RTC state. No normalized invalid dates.
inline bool utc(const char *s, uint32_t &epoch) {
  if (!s || strlen(s)!=20 || s[4]!='-'||s[7]!='-'||s[10]!='T'||
      s[13]!=':'||s[16]!=':'||s[19]!='Z') return false;
  unsigned val[6]={},pos[6]={0,5,8,11,14,17},width[6]={4,2,2,2,2,2};
  for (unsigned k=0;k<6;++k) for(unsigned i=0;i<width[k];++i) {
    const char c=s[pos[k]+i]; if(c<'0'||c>'9') return false; val[k]=val[k]*10+c-'0';
  }
  const unsigned y=val[0],m=val[1],d=val[2];
  if(y<1970||y>2099||m<1||m>12||!d||val[3]>23||val[4]>59||val[5]>59) return false;
  const unsigned md[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap=y%4==0 && (y%100!=0 || y%400==0);
  if(d>md[m-1]+(m==2&&leap?1U:0U)) return false;
  uint32_t days=0;
  for(unsigned yy=1970;yy<y;++yy) days+=365+(yy%4==0&&(yy%100!=0||yy%400==0));
  for(unsigned mm=1;mm<m;++mm) days+=md[mm-1]+(mm==2&&leap?1U:0U);
  epoch=((days+d-1)*24+val[3])*3600+val[4]*60+val[5]; return true;
}
} // namespace RemotePolicy
