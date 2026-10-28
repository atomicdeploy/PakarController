#include "../firmware/esp32s3/src/SshPolicy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
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
  std::puts("SSH policy: reserved ports, stale session/auth fencing, rollover deadlines and bounded PTY sizes passed; not SSH wire/device acceptance.");
}
