#include "TonePlayer.h"
#include "BootMelody.h"
#include <avr/interrupt.h>
#include <assert.h>
#include <stdio.h>
int main() {
  TonePlayer player; player.begin(); assert(!player.isBusy());
  playBootMelody(player); player.update(0); assert(player.activeFrequencyHz() == 1032);
  assert(OCR1A == (1000000UL + 516) / 1032 - 1);
  player.update(69); assert(player.activeFrequencyHz() == 1032);
  player.update(82); assert(player.activeFrequencyHz() == 0); // late, but pause ends at 130
  player.update(130); assert(player.activeFrequencyHz() == 2010);
  player.update(200); assert(player.activeFrequencyHz() == 0);
  player.update(260); assert(player.activeFrequencyHz() == 2400);
  player.update(380); assert(player.activeFrequencyHz() == 0);
  player.update(529); assert(player.isBusy());
  player.update(530); assert(!player.isBusy() && player.activeFrequencyHz() == 0 && player.activeDurationMs() == 0);
  playBootMelody(player); const uint32_t start=0xffffff00UL; player.update(start);
  player.update(start+270); assert(player.activeFrequencyHz() == 2400); // skipped expired steps, rollover
  player.update(start+530); assert(!player.isBusy());
  assert(!player.enqueue(19, 100) && !player.enqueue(20001, 100) && !player.enqueue(1000, 0));
  for(unsigned i=0;i<10;++i) assert(player.enqueue(1000,10));
  assert(!player.enqueue(1000,10)); player.update(1000); assert(TCCR1B != 0);
  player.setMuted(true); assert((TCCR1B & (_BV(CS10)|_BV(CS11)|_BV(CS12))) == 0);
  player.setMuted(false); assert(TCCR1B != 0);
  player.update(1100); assert(!player.isBusy()); player.stop(); assert(!player.isBusy());
  puts("Actual TonePlayer/BootMelody: exact 530 ms welcome, delayed-service deadlines, skipped expired notes, rollover, divider, capacity, mute/resume and idle readback passed (register double; not audible proof).");
}
