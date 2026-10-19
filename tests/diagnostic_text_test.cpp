#include <DiagnosticText.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <initializer_list>
using namespace BoardCore::DiagnosticText;
int main() {
  uint8_t all[256]; for (unsigned i=0;i<256;++i) all[i]=static_cast<uint8_t>(i);
  char encoded[513]; assert(encodeHex(all,sizeof(all),encoded,sizeof(encoded)));
  for(unsigned i=0;i<256;++i) {
    char expected[3]; snprintf(expected,sizeof(expected),"%02x",i);
    assert(memcmp(encoded+i*2,expected,2)==0);
  }
  assert(strlen(encoded)==512);
  char shortOut[3]="xx"; assert(!encodeHex(all,2,shortOut,sizeof(shortOut))&&shortOut[0]==0);
  assert(!encodeHex(all,SIZE_MAX,shortOut,sizeof(shortOut))&&shortOut[0]==0);
  assert(encodeHex(nullptr,0,shortOut,sizeof(shortOut))&&shortOut[0]==0);
  assert(!encodeHex(nullptr,1,shortOut,sizeof(shortOut)));
  assert(!encodeHex(all,1,nullptr,3));
  char text[40];
  assert(formatUptime(0,text,sizeof(text))&&!strcmp(text,"00:00:00"));
  assert(formatUptime(86399,text,sizeof(text))&&!strcmp(text,"23:59:59"));
  assert(formatUptime(86400,text,sizeof(text))&&!strcmp(text,"1 days, 00:00:00"));
  assert(formatUptime(UINT32_MAX,text,sizeof(text))&&!strcmp(text,"49710 days, 06:28:15"));
  assert(!formatUptime(0,shortOut,sizeof(shortOut))&&shortOut[0]==0);
  for(const auto &item : {0U,1023U,1024U,1048576U,UINT32_MAX}) {
    assert(formatBytes(item,text,sizeof(text)));
    if(item==0)assert(!strcmp(text,"0 B"));
    if(item==1023)assert(!strcmp(text,"1023 B"));
    if(item==1024)assert(!strcmp(text,"1 KB"));
    if(item==1048576)assert(!strcmp(text,"1.00 MB"));
    if(item==UINT32_MAX)assert(!strcmp(text,"4.00 GB"));
  }
  assert(!formatBytes(1024,shortOut,sizeof(shortOut))&&shortOut[0]==0);
  puts("PU850 actual diagnostic text: all byte values, capacity/overflow, uptime/day and unit boundaries passed.");
}
