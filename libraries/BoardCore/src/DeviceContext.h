#pragma once
#include "Protocol.h"

// One acknowledged, current-peer contract for JSON, USB and UI. Never derive a
// board profile from its hostname, pixel count, saved client settings or version.
#ifndef PAKAR_TABLE_ENABLED
#define PAKAR_TABLE_ENABLED 1
#endif
#ifndef PAKAR_FIRST_WINS_ENABLED
#define PAKAR_FIRST_WINS_ENABLED 1
#endif
namespace BoardCore {
namespace Context {
enum Feature : uint16_t {
  Network = 1U << 0, Telemetry = 1U << 1, Leds = 1U << 2,
  Streaming = 1U << 3, Table = 1U << 4, Display = 1U << 5,
  Text = 1U << 6, Buzzer = 1U << 7, Inputs = 1U << 8,
  Outputs = 1U << 9, Settings = 1U << 10, Isp = 1U << 11,
  Ota = 1U << 12, Identity = 1U << 13
};
inline const char *name(uint8_t bit) {
  static const char *const names[] = {"network", "telemetry", "leds", "streaming",
    "table", "display", "text", "buzzer", "inputs", "outputs", "settings", "isp", "ota", "identity"};
  return bit < 14 ? names[bit] : nullptr;
}
inline uint16_t features(bool helloFresh, uint16_t avrCaps, bool displayAck,
                         bool statusFresh, bool pcSupported) {
  uint16_t result = Network | Telemetry | Leds | Streaming | Isp | Ota | Identity;
  if (PAKAR_TABLE_ENABLED) result |= Table;
  if (!helloFresh) return result;
  if (avrCaps & Protocol::kCapBuzzer) result |= Buzzer;
  if (avrCaps & Protocol::kCapShiftInputs) result |= Inputs;
  if (avrCaps & Protocol::kCapShiftOutputs) result |= Outputs;
  if (pcSupported) result |= Settings;
  if (statusFresh && displayAck && (avrCaps & Protocol::kCapTm1637)) {
    result |= Display;
    if (avrCaps & Protocol::kCapDisplayText) result |= Text;
  }
  return result;
}
inline uint8_t modes(uint16_t available) {
  if (!(available & Table)) return 0;
  // Cell presentation belongs to the ESP table. First wins also needs
  // the acknowledged AVR inputs and outputs, not just a connected ESP.
  return 2U | 8U |
    ((PAKAR_FIRST_WINS_ENABLED && (available & (Inputs | Outputs)) == (Inputs | Outputs)) ? 4U : 0U);
}
inline const char *modeName(uint8_t code) {
  static const char *const names[] = {nullptr, "cell", "first-wins", "snake"};
  return code < 4 ? names[code] : nullptr;
}
inline const char *modeLabel(uint8_t code) {
  static const char *const names[] = {nullptr, "Cell selection", "First wins", "Snake"};
  return code < 4 ? names[code] : nullptr;
}
}
}
