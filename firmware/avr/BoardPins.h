#pragma once

#include <Arduino.h>

namespace BoardPins {

constexpr uint8_t ShiftDataIn = PIN_PC0;       // A0 / 74HC165 QH
constexpr uint8_t ShiftOutputLatch = PIN_PC1;  // A1 / 74HC595 RCLK
constexpr uint8_t ShiftClock = PIN_PC2;        // A2 / shared clock
constexpr uint8_t ShiftDataOut = PIN_PC3;      // A3 / 74HC595 SER
constexpr uint8_t ShiftInputLoad = PIN_PD4;    // D4 / 74HC165 /PL
constexpr uint8_t ShiftMasterReset = PIN_PD5;  // D5 / 74HC595 /SRCLR
constexpr uint8_t ShiftClockEnable = PIN_PD7;  // D7 / 74HC165 /CE
constexpr uint8_t ShiftOutputEnable = PIN_PB0; // D8 / 74HC595 /OE

constexpr uint8_t Buzzer = PIN_PB1;            // D9
constexpr uint8_t Tm1637Data = PIN_PB3;         // D11 / ISP MOSI
constexpr uint8_t Tm1637Clock = PIN_PB5;        // D13 / ISP SCK
constexpr uint8_t EspSerialRx = PIN_PD2;         // D2 <- ESP D0 / GPIO16
constexpr uint8_t EspSerialTx = PIN_PD3;         // D3 -> level shifter -> ESP D4 / GPIO2
constexpr uint8_t NinthButtonAdc = A6;          // ADC6, external pull-up required
constexpr uint16_t NinthButtonThreshold = 512;

}  // namespace BoardPins
