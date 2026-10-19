#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Release support, not an API generation or exact-build negotiation. Keep
// routes/opcodes living and check capabilities independently of this policy.
#ifndef PAKAR_ENFORCE_PEER_VERSIONS
#define PAKAR_ENFORCE_PEER_VERSIONS 1
#endif
#if PAKAR_ENFORCE_PEER_VERSIONS != 0 && PAKAR_ENFORCE_PEER_VERSIONS != 1
#error "PAKAR_ENFORCE_PEER_VERSIONS must be 0 or 1"
#endif
#ifndef PAKAR_MIN_APP_VERSION
#define PAKAR_MIN_APP_VERSION "0.2.8"
#endif
#ifndef PAKAR_MIN_BOARD_VERSION
#define PAKAR_MIN_BOARD_VERSION "0.2.8"
#endif
#ifndef PAKAR_MAX_APP_VERSION
#define PAKAR_MAX_APP_VERSION ""
#endif
#ifndef PAKAR_MAX_BOARD_VERSION
#define PAKAR_MAX_BOARD_VERSION ""
#endif

namespace BoardCore { namespace PeerCompatibility {
constexpr bool enforced = PAKAR_ENFORCE_PEER_VERSIONS != 0;
constexpr char minimumApp[] = PAKAR_MIN_APP_VERSION;
constexpr char minimumBoard[] = PAKAR_MIN_BOARD_VERSION;
constexpr char maximumApp[] = PAKAR_MAX_APP_VERSION;
constexpr char maximumBoard[] = PAKAR_MAX_BOARD_VERSION;
struct NumericBound { uint32_t parts[3]{}; bool valid=false; };
constexpr NumericBound numericBound(const char *text) {
  NumericBound value;
  for(unsigned part=0;part<3;++part) {
    const char *start=text;
    if(*text<'0'||*text>'9')return value;
    while(*text>='0'&&*text<='9') {
      const uint32_t n=static_cast<uint32_t>(*text-'0');
      if(value.parts[part]>(UINT32_MAX-n)/10U)return value;
      value.parts[part]=value.parts[part]*10U+n;++text;
    }
    if(text-start>1&&*start=='0')return value;
    if(part<2&&*text++!='.')return value;
  }
  value.valid=*text==0;return value;
}
constexpr int numericCompare(uint8_t major,uint8_t minor,uint8_t patch,const NumericBound &bound) {
  // Explicit fields let AVR's compiler fold stable build bounds without
  // emitting a runtime uint32 array/loop for three byte-sized wire fields.
  return major!=bound.parts[0]?(major<bound.parts[0]?-1:1):
      minor!=bound.parts[1]?(minor<bound.parts[1]?-1:1):
      patch!=bound.parts[2]?(patch<bound.parts[2]?-1:1):0;
}
// AVR HELLO's fixed wire fields are numeric, stable release triples. Evaluate
// its configured bounds at COMPILE time: no sprintf, heap, or full SemVer parser
// in an already-full ATmega image. ESP/apps retain the full string policy below.
inline bool numericAppAllowed(uint8_t major,uint8_t minor,uint8_t patch) {
  constexpr auto lower=numericBound(minimumApp),upper=numericBound(maximumApp);
  static_assert(!enforced||lower.valid,"Numeric UART minimum must be a stable major.minor.patch");
  static_assert(!enforced||!*maximumApp||upper.valid,"Numeric UART maximum must be a stable major.minor.patch");
  return !enforced||(numericCompare(major,minor,patch,lower)>=0 &&
      (!*maximumApp||numericCompare(major,minor,patch,upper)<0));
}
struct Version { uint32_t parts[3]{}; const char *prerelease=nullptr; size_t length=0; };
inline bool digit(char c) { return c>='0' && c<='9'; }
inline bool identifier(char c) {
  return digit(c)||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-';
}
inline bool parse(const char *text,Version &result) {
  if(!text || !*text || strlen(text)>63)return false;
  Version value;const char *p=text;
  for(unsigned part=0;part<3;++part) {
    const char *start=p;
    if(!digit(*p))return false;
    while(digit(*p)) {
      const uint32_t n=static_cast<uint32_t>(*p-'0');
      if(value.parts[part]>(UINT32_MAX-n)/10U)return false;
      value.parts[part]=value.parts[part]*10U+n;++p;
    }
    if(p-start>1 && *start=='0')return false;
    if(part<2 && *p++!='.')return false;
  }
  if(*p=='-') {
    value.prerelease=++p;
    while(true) {
      const char *start=p;bool numeric=true;
      while(identifier(*p)){numeric=numeric&&digit(*p);++p;}
      if(p==start || (numeric&&p-start>1&&*start=='0'))return false;
      if(*p!='.')break;
      ++p;
    }
    value.length=static_cast<size_t>(p-value.prerelease);
  }
  if(*p=='+') {
    ++p;
    while(true) {
      const char *start=p;while(identifier(*p))++p;
      if(p==start)return false;
      if(*p!='.')break;
      ++p;
    }
  }
  if(*p)return false;
  result=value;return true;
}
inline int compare(const Version &a,const Version &b) {
  for(unsigned part=0;part<3;++part)
    if(a.parts[part]!=b.parts[part])return a.parts[part]<b.parts[part]?-1:1;
  if(!a.prerelease || !b.prerelease)
    return a.prerelease?(b.prerelease?0:-1):(b.prerelease?1:0);
  size_t ai=0,bi=0;
  while(ai<a.length && bi<b.length) {
    const size_t as=ai,bs=bi;bool an=true,bn=true;
    while(ai<a.length&&a.prerelease[ai]!='.'){an=an&&digit(a.prerelease[ai]);++ai;}
    while(bi<b.length&&b.prerelease[bi]!='.'){bn=bn&&digit(b.prerelease[bi]);++bi;}
    const size_t al=ai-as,bl=bi-bs;
    if(an!=bn)return an?-1:1;
    if(an&&al!=bl)return al<bl?-1:1;
    const size_t count=al<bl?al:bl;
    const int cmp=memcmp(a.prerelease+as,b.prerelease+bs,count);
    if(cmp)return cmp<0?-1:1;
    if(al!=bl)return al<bl?-1:1;
    if(ai<a.length)++ai;
    if(bi<b.length)++bi;
  }
  return ai<a.length?1:bi<b.length?-1:0;
}
inline bool supported(const char *version,const char *minimum,const char *maximum="") {
  Version candidate,lower,upper;
  return parse(version,candidate)&&parse(minimum,lower)&&compare(candidate,lower)>=0 &&
      (!maximum||!*maximum||(parse(maximum,upper)&&compare(candidate,upper)<0));
}
inline bool appAllowed(const char *version) {
  return !enforced||supported(version,minimumApp,maximumApp);
}
inline bool boardAllowed(const char *version) {
  return !enforced||supported(version,minimumBoard,maximumBoard);
}
} }
