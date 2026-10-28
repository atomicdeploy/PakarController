#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Wire values are UTC, never locale strings. HTTP dates are observations, not
// authenticated time unless the connection itself was authenticated.
namespace PakarNetworkObservation {
inline bool hostname(const char *s) {
  if(!s||!*s||strlen(s)>253)return false;
  size_t label=0;
  for(size_t i=0;s[i];++i){
    const char c=s[i];
    if(c=='.'){if(!label||s[i-1]=='-')return false;label=0;continue;}
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')||
        (!label&&c=='-')||++label>63)return false;
  }
  return label&&s[strlen(s)-1]!='-';
}
inline bool publicIpv4(const char *s,uint8_t *bytes=nullptr) {
  unsigned a,b,c,d;int used=0;
  if(!s||sscanf(s,"%u.%u.%u.%u%n",&a,&b,&c,&d,&used)!=4||s[used]||
      a>255||b>255||c>255||d>255)return false;
  char canonical[16];snprintf(canonical,sizeof(canonical),"%u.%u.%u.%u",a,b,c,d);
  if(strcmp(s,canonical)||a==0||a==10||a==127||a>=224||
      (a==100&&b>=64&&b<=127)||(a==169&&b==254)||(a==172&&b>=16&&b<=31)||
      (a==192&&(b==168||(b==0&&(c==0||c==2))||(b==88&&c==99)))||
      (a==198&&(b==18||b==19||(b==51&&c==100)))||
      (a==203&&b==0&&c==113))return false;
  if(bytes){bytes[0]=a;bytes[1]=b;bytes[2]=c;bytes[3]=d;}return true;
}
inline bool httpDate(const char *s,uint32_t &epoch) {
  // IMF-fixdate only. Reject trailing bytes, impossible dates, wrong weekdays,
  // leap seconds and old/future garbage rather than normalizing with mktime.
  if(!s||strlen(s)!=29||s[3]!=','||s[4]!=' '||s[7]!=' '||s[11]!=' '||
      s[16]!=' '||s[19]!=':'||s[22]!=':'||strcmp(s+25," GMT"))return false;
  const unsigned positions[]={5,6,12,13,14,15,17,18,20,21,23,24};
  for(unsigned i:positions)if(s[i]<'0'||s[i]>'9')return false;
  const auto two=[&](unsigned at){return unsigned(s[at]-'0')*10+unsigned(s[at+1]-'0');};
  const unsigned year=two(12)*100+two(14),day=two(5),hour=two(17),minute=two(20),second=two(23);
  if(year<2024||year>2099||hour>23||minute>59||second>59)return false;
  const char *months="JanFebMarAprMayJunJulAugSepOctNovDec";unsigned month=0;
  for(unsigned i=0;i<12;++i)if(!memcmp(s+8,months+i*3,3))month=i+1;
  if(!month)return false;
  constexpr unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap=year%4==0&&(year%100!=0||year%400==0);
  if(!day||day>days[month-1]+unsigned(month==2&&leap))return false;
  unsigned elapsed=0;
  for(unsigned y=1970;y<year;++y)elapsed+=365+unsigned(y%4==0&&(y%100!=0||y%400==0));
  for(unsigned m=1;m<month;++m)elapsed+=days[m-1]+unsigned(m==2&&leap);
  elapsed+=day-1;
  const char *week="SunMonTueWedThuFriSat";
  if(memcmp(s,week+((elapsed+4)%7)*3,3))return false;
  epoch=elapsed*86400UL+hour*3600+minute*60+second;return true;
}
} // namespace PakarNetworkObservation
