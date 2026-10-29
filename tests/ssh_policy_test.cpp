#include "../firmware/esp32s3/src/SshPolicy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <string>
int main(){
  using namespace PakarS3::SshPolicy;
  assert(portAllowed(22,23)&&portAllowed(2222,23));
  for(auto port:{0,23,80,443,7777})assert(!portAllowed(port,23));
  assert(admitted(7,7,2,2));
  assert(!admitted(0,0,2,2)&&!admitted(7,8,2,2)&&!admitted(7,7,2,3)&&!admitted(7,7,0,0));
  assert(expired(100,50,50)&&!expired(99,50,50));
  assert(expired(5,UINT32_MAX-5,11)&&!expired(4,UINT32_MAX-5,11));
  assert(columns(0)==40&&columns(9999)==240&&columns(120)==120);
  assert(rows(-2)==16&&rows(200)==100&&rows(30)==30);
  static_assert(packetBytes==128&&handshakeMs<loginMs&&drainMs<idleMs);
  assert(commandLength("status")==6&&commandLength(" tui ")==5);
  assert(commandLength(std::string(128,'x').c_str())==128);
  assert(!commandLength(std::string(129,'x').c_str()));
  for(const char* bad:{""," ","status\nreset","status\rreset","\x1b[H","status\t","\x7f","\xc3\xa9"})assert(!commandLength(bad));
  assert(!commandLength(nullptr));
  std::puts("SSH policy: reserved ports, stale session/auth fencing, rollover deadlines and bounded PTY sizes passed; not SSH wire/device acceptance.");
}
