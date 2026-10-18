#include "BootMelody.h"

void playBootMelody(TonePlayer &player) {
  player.stop();

  player.enqueue(1032, 70);
  player.pause(60);
  player.enqueue(2010, 70);
  player.pause(60);
  player.enqueue(2400, 120);
  player.pause(150);
}

bool playAudioCue(BoardCore::Protocol::AudioCue cue, TonePlayer &player) {
  using namespace BoardCore::Protocol;
  if (cue >= kCueCount) return false;
  player.stop();
  switch (cue) {
    case kCueWelcome: playBootMelody(player); break;
    case kCueWelcomePuzzleBoard:
    case kCueWelcome40Led:
      // Actual PuzzleBoard::validBeep and 40LED::setup welcome schedules.
      // The 40LED variant has a final additional 70 ms silence. Keep both,
      // without importing their blocking delays or mismatched buzzer pins.
      player.enqueue(1032, 70); player.pause(70);
      player.enqueue(2010, 70); player.pause(70);
      player.enqueue(2400, 90); player.pause(150);
      if (cue == kCueWelcome40Led) player.pause(70);
      break;
    case kCueUpdateStart:
      player.enqueue(NOTE_G5,90); player.pause(45);
      player.enqueue(NOTE_E5,90); player.pause(45); player.enqueue(NOTE_C5,180); break;
    case kCueUpdateSuccess:
      player.enqueue(NOTE_C5,80); player.pause(35);
      player.enqueue(NOTE_E5,90); player.pause(35); player.enqueue(NOTE_G5,170); break;
    case kCueAttention:
      player.enqueue(NOTE_C6,90); player.pause(60); player.enqueue(NOTE_E6,90); break;
    case kCueError: player.error(); break;
    case kCueWinner: player.enqueue(NOTE_C6,90); player.pause(35); player.enqueue(NOTE_G6,180); break;
    case kCueFinish: player.enqueue(NOTE_E5,100); player.enqueue(NOTE_G5,100); player.enqueue(NOTE_A5,250); break;
    case kCueLost:
      player.enqueue(NOTE_G4,100); player.enqueue(NOTE_E4,100);
      player.enqueue(NOTE_C4,100); player.enqueue(NOTE_G3,100); break;
    case kCueIncorrect:
      for (uint8_t i=0;i<3;++i) { player.enqueue(2000,100); player.pause(100); } break;
    case kCueRapidError:
      for (uint8_t i=0;i<5;++i) { player.enqueue(2000,10); player.pause(10); } break;
    case kCueFault: player.enqueue(1000,250); player.enqueue(500,500); player.pause(5000); break;
    case kCueIncrease:
      player.enqueue(2000,100); player.pause(50); player.enqueue(2500,100); player.pause(50); break;
    case kCueDecrease:
      player.enqueue(1500,100); player.pause(50); player.enqueue(1000,100); player.pause(50); break;
    default: return false;
  }
  return true;
}
