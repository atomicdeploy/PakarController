#include "MenuAudio.h"
#include "BoardPins.h"
#include <cassert>
#include <cstdio>
using namespace BoardCore::Protocol;
struct Sender {
  bool writable = true;
  unsigned writes = 0;
  uint8_t cue = 0;
  bool send(uint8_t opcode, uint8_t, const uint8_t *payload, uint8_t length) {
    assert(opcode == kAudioCue && length == 1);
    if (!writable) return false;
    ++writes; cue = payload[0]; return true;
  }
};
int main() {
  static_assert(PakarS3::safePins());
  PakarS3::MenuAudio sound; Sender sender;
  assert(!sound.navigate(sender, 1, true, 0)); // No fabricated hardware.
  Frame hello{}; hello.opcode = kHelloResponse; hello.payloadLength = 13;
  hello.payload[1] = kRoleAvr; hello.payload[2] = kCapBuzzer;
  hello.payload[12] = kCueCount;
  sound.observe(hello, 0);
  assert(sound.navigate(sender, 1, true, 1));
  assert(sender.cue == kCueIncrease && !sound.accepted());
  assert(!sound.navigate(sender, 2, false, 2)); // One bounded in-flight cue.
  Frame ack{}; ack.opcode = kAck; ack.sequence = 9;
  ack.payloadLength = 2; ack.payload[0] = kAudioCue;
  sound.observe(ack, 3); assert(sound.pending() && !sound.accepted());
  ack.sequence = 1; sound.observe(ack, 4);
  assert(!sound.pending() && sound.accepted());
  assert(!sound.navigate(sender, 2, false, 5)); // Debounce.
  assert(sound.navigate(sender, 2, false, 90));
  assert(sender.cue == kCueDecrease);
  ack.sequence = 2; sound.observe(ack, 341); // Late ACK cannot claim success.
  assert(!sound.pending() && !sound.accepted());
  sender.writable = false;
  assert(!sound.navigate(sender, 3, true, 400));
  assert(!sound.pending()); sender.writable = true;
  sound.setEnabled(false); assert(!sound.navigate(sender, 3, true, 500));
  sound.setEnabled(true); assert(!sound.navigate(sender, 3, true, 3001));
  hello.payload[12] = 3; sound.observe(hello, 3002);
  assert(!sound.navigate(sender, 3, true, 3003)); // Unsupported cue ID.
  hello.payload[12] = kCueCount; hello.payload[2] = 0;
  sound.observe(hello, 3004); assert(!sound.navigate(sender, 3, true, 3005));
  hello.payload[2] = kCapBuzzer; sound.observe(hello, 3006);
  sound.disconnected(); assert(!sound.navigate(sender, 3, true, 3007));
  assert(sender.writes == 2);
  puts("S3 menu-audio contract: feature/role ACK, cue bounds, correlation, deadline, debounce, disabled/offline and no-write failure pass; not physical playback.");
}
