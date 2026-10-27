#pragma once
#include "Protocol.h"

namespace BoardCore {
namespace AudioCues {
constexpr uint8_t kSilent = 255;
inline bool validGameCue(uint8_t cue) { return cue == kSilent || cue < Protocol::kCueCount; }
inline const char *name(uint8_t cue) {
  switch (cue) {
    case Protocol::kCueWelcome: return "welcome";
    case Protocol::kCueUpdateStart: return "update-start";
    case Protocol::kCueUpdateSuccess: return "update-success";
    case Protocol::kCueError: return "error";
    case Protocol::kCueWinner: return "winner";
    case Protocol::kCueFinish: return "finish";
    case Protocol::kCueLost: return "lost";
    case Protocol::kCueIncorrect: return "incorrect";
    case Protocol::kCueRapidError: return "rapid-error";
    case Protocol::kCueFault: return "fault";
    case Protocol::kCueIncrease: return "increase";
    case Protocol::kCueDecrease: return "decrease";
    case Protocol::kCueWelcomePuzzleBoard: return "welcome-puzzleboard";
    case Protocol::kCueWelcome40Led: return "welcome-40led";
    case Protocol::kCueAttention: return "attention";
    default: return nullptr;
  }
}
inline bool parse(const char *value, uint8_t &cue) {
  if (!value) return false;
  for (uint8_t i = 0; i < Protocol::kCueCount; ++i) {
    if (strcmp(value, name(i)) == 0) { cue = i; return true; }
  }
  return false;
}
inline const char *gameName(uint8_t cue) { return cue == kSilent ? "silent" : name(cue); }
inline bool parseGame(const char *value, uint8_t &cue) {
  if(value && strcmp(value,"silent")==0){cue=kSilent;return true;}
  return parse(value,cue);
}
// Stable cue IDs, enabled only by this peer's explicit HELLO advertisement.
// Build versions are descriptive and never substitute for feature ACKs.
inline uint8_t supportedCount(bool online, bool acknowledged, uint8_t advertised) {
  if (!online || !acknowledged) return 0;
  return advertised < Protocol::kCueCount ? advertised : static_cast<uint8_t>(Protocol::kCueCount);
}
}  // namespace AudioCues
}  // namespace BoardCore
