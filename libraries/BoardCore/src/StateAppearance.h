#pragma once
#include <stdint.h>
#include <string.h>
#include "Protocol.h"
#include "KeyPresentation.h"

namespace BoardCore {
struct StateAppearance {
  struct Style {
    uint32_t rgb;
    uint8_t motion = KeyPresentation::Solid;
  } states[4] = {{0xffffff}, {0xff0000}, {0}, {0x00ff00}};
  bool enabled = false, neighbors = true, invert = false, diagonal = false;
  uint16_t pulseMs = 700, ringMs = 900, chaseMs = 1800, fadeMs = 300;
  static const char *name(uint8_t state) {
    switch(state) {
      case Protocol::kCellIdle: return "idle";
      case Protocol::kCellLost: return "lost";
      case Protocol::kCellWon: return "won";
      case Protocol::kCellOff: return "off";
      default: return nullptr;
    }
  }
  static bool parse(const char *name, Protocol::CellState &state) {
    if (!name) return false;
    for (uint8_t i=0;i<4;++i) if (!strcmp(name,StateAppearance::name(i))) {
      state=static_cast<Protocol::CellState>(i); return true;
    }
    // Existing unversioned callers retain their semantic intent.
    if (!strcmp(name,"white")) {state=Protocol::kCellIdle;return true;}
    if (!strcmp(name,"red")) {state=Protocol::kCellLost;return true;}
    return false;
  }
  bool valid() const {
    if (pulseMs<100 || pulseMs>10000 || ringMs<100 || ringMs>20000 ||
        chaseMs<100 || chaseMs>20000 || fadeMs>10000) return false;
    for (const auto &style:states) if(style.rgb>0xffffff || style.motion>KeyPresentation::SharedChase) return false;
    return states[Protocol::kCellOff].rgb==0 && states[Protocol::kCellOff].motion==KeyPresentation::Solid;
  }
};
}
