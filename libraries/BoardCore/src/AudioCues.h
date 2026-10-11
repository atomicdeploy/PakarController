#pragma once
#include "Protocol.h"

namespace BoardCore {
namespace AudioCues {
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
// Stable cue IDs, enabled only by this peer's explicit HELLO advertisement.
// Build versions are descriptive and never substitute for feature ACKs.
inline uint8_t supportedCount(bool online, bool acknowledged, uint8_t advertised) {
  if (!online || !acknowledged) return 0;
  return advertised < Protocol::kCueCount ? advertised : static_cast<uint8_t>(Protocol::kCueCount);
}
}  // namespace AudioCues
}  // namespace BoardCore
