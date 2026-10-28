#include "TonePlayer.h"
#include "BootMelody.h"
#include "AudioCues.h"
#include "FlashToneSequence.h"
#include <avr/interrupt.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

// Compare actual queued playback, not names or total duration alone. Every
// step has millisecond resolution; keep all silent samples, including the
// trailing pause, because they change busy/completion and mirroring timing.
using CueTimeline = std::vector<uint16_t>;

static CueTimeline captureTimeline(TonePlayer &player, uint32_t start) {
  CueTimeline timeline;
  for (uint32_t ms = 0; ms <= 6000; ++ms) {
    player.update(start + ms);
    if (!player.isBusy()) return timeline;
    timeline.push_back(player.activeFrequencyHz());
  }
  assert(false && "A finite melody must complete within the catalog budget");
  return timeline;
}

int main() {
  TonePlayer player; player.begin(); assert(!player.isBusy());
  playBootMelody(player); player.update(0); assert(player.activeFrequencyHz() == 1032);
  assert(OCR1A == (1000000UL + 516) / 1032 - 1);
  player.update(69); assert(player.activeFrequencyHz() == 1032);
  player.update(82); assert(player.activeFrequencyHz() == 0); // late, but pause ends at 130
  assert(player.activeStartedMicros() == 70000); // original cadence, not late service time
  player.update(130); assert(player.activeFrequencyHz() == 2010);
  player.update(200); assert(player.activeFrequencyHz() == 0);
  player.update(260); assert(player.activeFrequencyHz() == 2400);
  player.update(380); assert(player.activeFrequencyHz() == 0);
  player.update(529); assert(player.isBusy());
  player.update(530); assert(!player.isBusy() && player.activeFrequencyHz() == 0 && player.activeDurationMs() == 0);
  playBootMelody(player); const uint32_t start=0xffffff00UL; player.update(start);
  player.update(start+270); assert(player.activeFrequencyHz() == 2400); // skipped expired steps, rollover
  assert(player.activeStartedMicros() == (start + 260U) * 1000U);
  player.update(start+530); assert(!player.isBusy());
  assert(!player.enqueue(19, 100) && !player.enqueue(20001, 100) && !player.enqueue(1000, 0));
  for(unsigned i=0;i<10;++i) assert(player.enqueue(1000,10));
  assert(!player.enqueue(1000,10)); player.update(1000); assert(TCCR1B != 0);
  const auto beforeMute = player.revision();
  player.setMuted(true); assert((TCCR1B & (_BV(CS10)|_BV(CS11)|_BV(CS12))) == 0);
  assert(player.muted() && player.revision() != beforeMute);
  const auto mutedRevision = player.revision(); player.setMuted(true);
  assert(player.revision() == mutedRevision); // Idempotent settings refresh.
  player.setMuted(false); assert(!player.muted());
  assert((TCCR1B & (_BV(CS10)|_BV(CS11)|_BV(CS12))) == 0);
  assert(player.revision() != mutedRevision); // No replay of interrupted note.
  player.update(1100); assert(!player.isBusy()); player.stop(); assert(!player.isBusy());
  // Remote chunks prefill the existing ring; network arrivals never start notes.
  uint8_t beginStream[]={0,1,0,0,0}, status[7];
  uint8_t chunk[]={1,1,0,0,0,0xe8,3,100,0,0,0,50,0};
  uint8_t startStream[]={2,1,0,2,0}, finishStream[]={3,1,0,2,0};
  assert(player.stream(beginStream,5)==0);
  assert(player.stream(chunk,sizeof(chunk))==0);
  player.update(1200);assert(player.activeFrequencyHz()==0);
  assert(player.stream(chunk,sizeof(chunk))==6); // No duplicate append replay.
  assert(player.stream(finishStream,5)==0);
  assert(player.stream(startStream,5)==0);
  player.update(1300);assert(player.activeFrequencyHz()==1000);
  player.update(1410);assert(player.activeFrequencyHz()==0);
  player.update(1450);player.streamStatus(status);assert(status[5]==3&&status[4]==10);
  beginStream[1]=2;chunk[1]=2;startStream[1]=2;
  assert(player.stream(beginStream,5)==0);
  assert(player.stream(chunk,sizeof(chunk))==0);
  assert(player.stream(startStream,5)==0);
  player.update(1500);player.update(1650);
  player.streamStatus(status);assert(status[5]==4); // Underrun stays silent.
  chunk[3]=2;assert(player.stream(chunk,sizeof(chunk))==6);
  player.stop();
  // The complete flash timeline runs without any further network packets.
  uint8_t flash[]={6,3,0,0,0,120,0,100,0,4,144,1};
  assert(player.stream(flash,sizeof(flash))==0);
  assert(player.isBusy());player.update(0);assert(player.activeFrequencyHz()==1047);
  player.streamStatus(status);assert(status[5]==2&&status[4]==0);
  assert(player.stream(flash,sizeof(flash))==6); // No restart on duplicate.
  assert(!player.enqueue(2000,20)); // Cannot overwrite timeline storage.
  player.update(119);assert(player.activeFrequencyHz()==1047);
  player.update(120);assert(player.activeFrequencyHz()==0);
  player.update(220);assert(player.activeFrequencyHz()==1319);
  player.update(450);assert(player.activeFrequencyHz()==1568);
  player.update(670);assert(player.activeFrequencyHz()==2093);
  player.update(1059);assert(player.activeFrequencyHz()==2093);
  player.update(1060);assert(player.activeFrequencyHz()==0&&player.isBusy());
  player.update(1160);assert(!player.isBusy());
  player.streamStatus(status);assert(status[5]==3);
  flash[1]=4;flash[5]=0;assert(player.stream(flash,sizeof(flash))==3);
  flash[5]=120;assert(player.stream(flash,sizeof(flash))==0);
  player.setMuted(true);player.update(50);assert((TCCR1B & (_BV(CS10)|_BV(CS11)|_BV(CS12)))==0);
  player.stop();assert(!player.isBusy());player.setMuted(false);
  uint8_t finite[]={7,5,0,0,0,11,2,55,0,142,2,55,0,16,3,100,0};
  assert(player.stream(finite,sizeof(finite))==0);
  player.update(100);assert(player.activeFrequencyHz()==523);
  assert(player.stream(finite,sizeof(finite))==6); // No audible duplicate replay.
  finite[1]=6;finite[7]=0;assert(player.stream(finite,sizeof(finite))==3);
  player.update(155);assert(player.activeFrequencyHz()==654); // Invalid replacement left timeline intact.
  player.update(210);assert(player.activeFrequencyHz()==784);
  player.update(310);assert(!player.isBusy());
  finite[7]=55;assert(player.stream(finite,sizeof(finite))==0);
  player.stop();assert(!player.isBusy());
  using namespace BoardCore::Protocol;
  const uint16_t durations[kCueCount] = {530,450,410,300,1080,450,400,600,100,5750,300,300,520,590,240};
  CueTimeline timelines[kCueCount];
  for (uint8_t cue=0;cue<kCueCount;++cue) {
    assert(playAudioCue(static_cast<AudioCue>(cue), player));
    const uint32_t begin = 20000UL + cue * 10000UL;
    for (uint16_t ms=0;ms<durations[cue];++ms) {
      player.update(begin+ms); assert(player.isBusy());
      timelines[cue].push_back(player.activeFrequencyHz());
    }
    player.update(begin+durations[cue]); assert(!player.isBusy());
    uint8_t parsed=255;
    assert(BoardCore::AudioCues::parse(BoardCore::AudioCues::name(cue),parsed) && parsed==cue);
    player.setMuted(true);
    assert(playAudioCue(static_cast<AudioCue>(cue), player));
    for (uint16_t ms=0;ms<=durations[cue];++ms) {
      player.update(begin+ms);
      assert((TCCR1B & (_BV(CS10)|_BV(CS11)|_BV(CS12))) == 0);
    }
    assert(!player.isBusy()); player.setMuted(false);
  }
  // Distinct names/IDs must not conceal exactly duplicated melodies. Check
  assert(timelines[kCueWinner][0]==NOTE_C6 && timelines[kCueWinner][239]==NOTE_C6);
  assert(timelines[kCueWinner][240]==0 && timelines[kCueWinner][359]==0);
  assert(timelines[kCueWinner][360]==NOTE_E6 && timelines[kCueWinner][599]==NOTE_E6);
  assert(timelines[kCueWinner][600]==0 && timelines[kCueWinner][719]==0);
  assert(timelines[kCueWinner][720]==NOTE_G6 && timelines[kCueWinner][1079]==NOTE_G6);
  // all pairs, including equal-duration entries and the donor welcome variants.
  for (uint8_t cue = 0; cue < kCueCount; ++cue) {
    for (uint8_t other = 0; other < cue; ++other) {
      if (timelines[cue] == timelines[other]) {
        fprintf(stderr, "Exact duplicate melodies: %s and %s\n",
                BoardCore::AudioCues::name(cue), BoardCore::AudioCues::name(other));
        assert(false && "Consolidate duplicate melody definitions");
      }
    }
  }
  // These are intentional shared entry points, not independent catalog copies.
  playBootMelody(player);
  assert(captureTimeline(player, 1000000) == timelines[kCueWelcome]);
  player.error();
  assert(captureTimeline(player, 2000000) == timelines[kCueError]);
  // The private success helper resembles attention/update-success, but its
  // exact notes and 210 ms cadence are a separate, nonduplicated schedule.
  player.success();
  const auto success = captureTimeline(player, 3000000);
  assert(success.size() == 210);
  for (const auto &timeline : timelines) assert(success != timeline);
  // Protect the comparator against overlooking pitches, internal/trailing
  // silence, or an equal total duration with different musical timing.
  auto changed = timelines[kCueWelcome];
  changed[0] += 1;
  assert(changed != timelines[kCueWelcome]);
  changed = timelines[kCueWelcome];
  changed[69] = 0; changed[70] = 1032;
  assert(changed.size() == timelines[kCueWelcome].size() &&
         changed != timelines[kCueWelcome]);
  changed = timelines[kCueWelcome]; changed.pop_back();
  assert(changed != timelines[kCueWelcome]);
  assert(playAudioCue(kCueWelcomePuzzleBoard,player)); player.update(0);
  player.update(70); assert(player.activeFrequencyHz()==0);
  player.update(139); assert(player.activeFrequencyHz()==0);
  player.update(140); assert(player.activeFrequencyHz()==2010);
  player.update(280); assert(player.activeFrequencyHz()==2400);
  player.update(370); assert(player.activeFrequencyHz()==0);
  player.update(520); assert(!player.isBusy());
  assert(playAudioCue(kCueWelcome40Led,player)); player.update(0);
  player.update(520); assert(player.isBusy() && player.activeFrequencyHz()==0);
  player.update(590); assert(!player.isBusy());
  playBootMelody(player); player.update(0);
  assert(!playAudioCue(static_cast<AudioCue>(255),player)); assert(player.activeFrequencyHz()==1032);
  uint8_t unchanged=77; assert(!BoardCore::AudioCues::parse("bad",unchanged) && unchanged==77);
  uint8_t gameCue=77;
  assert(BoardCore::AudioCues::parseGame("silent",gameCue)&&gameCue==255);
  assert(strcmp(BoardCore::AudioCues::gameName(gameCue),"silent")==0);
  assert(BoardCore::AudioCues::validGameCue(gameCue));
  assert(!BoardCore::AudioCues::parse("silent",gameCue)); // Not a playable catalog cue.
  assert(!BoardCore::AudioCues::parseGame("",gameCue)&&gameCue==255);
  assert(!BoardCore::AudioCues::validGameCue(kCueCount));
  assert(!BoardCore::AudioCues::validGameCue(254));
  for(uint8_t cue=0;cue<kCueCount;++cue){
    assert(BoardCore::AudioCues::validGameCue(cue));
    assert(BoardCore::AudioCues::parseGame(BoardCore::AudioCues::gameName(cue),gameCue)&&gameCue==cue);
  }
  assert(!BoardCore::AudioCues::parse(nullptr,unchanged));
  assert(BoardCore::AudioCues::supportedCount(true,true,12)==12);
  assert(BoardCore::AudioCues::supportedCount(true,true,14)==14);
  assert(BoardCore::AudioCues::supportedCount(true,true,15)==15);
  assert(BoardCore::AudioCues::supportedCount(true,true,255)==15);
  assert(BoardCore::AudioCues::supportedCount(true,true,0)==0);
  assert(BoardCore::AudioCues::supportedCount(false,true,14)==0);
  assert(BoardCore::AudioCues::supportedCount(true,false,14)==0);
  puts("Actual TonePlayer/BootMelody: fifteen finite cues including attention and exact 530/520/590 ms donor welcome variants, deadlines, rollover, capacity, mute, identity-gated metadata passed (register double; not audible proof).");
  puts("Melody deduplication: all 105 catalog pairs differ in exact pitch/timing/silence; boot/error entry points share their existing definitions, and success is distinct.");
}
