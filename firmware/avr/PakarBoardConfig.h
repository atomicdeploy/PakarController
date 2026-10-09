#pragma once

#include <Arduino.h>

// PakarController-specific inputs layered on top of PCController's imported
// ControllerBoardMini pin map. ADC6 is analog-input-only on the ATmega328P.
namespace PakarBoardConfig {

constexpr uint8_t NinthButtonAdc = A6;
constexpr uint16_t NinthButtonThreshold = 512;

}  // namespace PakarBoardConfig
