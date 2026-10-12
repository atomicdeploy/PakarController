#include <cassert>
#include "../firmware/esp8266/src/Mndp.h"
int main() {
  PakarMndp::Packet p(0x1234);
  assert(p.data()[0]==0 && p.data()[1]==0 && p.data()[2]==0x12 && p.data()[3]==0x34);
  assert(p.text(5,"ledboard"));
  assert(p.size()==16 && p.data()[5]==5 && p.data()[7]==8);
  char large[255]{};
  assert(p.tlv(7,large,sizeof(large)));
  assert(!p.tlv(8,large,sizeof(large)) && p.size()==0);
  assert(!p.text(5,"never emit a partial advertisement"));
}
