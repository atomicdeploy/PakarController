#include <cassert>
#include "../firmware/esp/src/Mndp.h"
int main() {
  PakarMndp::AnnouncementSchedule schedule;
  schedule.begin(100);assert(schedule.due(100));assert(!schedule.due(101));
  schedule.request();schedule.request();assert(!schedule.due(349));assert(schedule.due(350));
  assert(!schedule.due(351));assert(!schedule.due(30349));assert(schedule.due(30350));
  schedule.begin(0xffffff80U);assert(schedule.due(0xffffff80U));
  schedule.request();assert(!schedule.due(0x79U));assert(schedule.due(0x7aU)); //250ms across rollover.
  PakarMndp::Packet p(0x1234);
  assert(p.data()[0]==0 && p.data()[1]==0 && p.data()[2]==0x12 && p.data()[3]==0x34);
  assert(p.text(5,"ledboard"));
  assert(p.size()==16 && p.data()[5]==5 && p.data()[7]==8);
  char large[255]{};
  assert(p.tlv(7,large,sizeof(large)));
  assert(!p.tlv(8,large,sizeof(large)) && p.size()==0);
  assert(!p.text(5,"never emit a partial advertisement"));
}
