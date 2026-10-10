#pragma once

#include <Arduino.h>

// ControllerBoardMini / ATmega328P pin ownership.
//
// The aliases use MiniCore's physical-port names so the mapping remains
// unambiguous even when a different Arduino pin-number variant is selected.
namespace BoardPins {

// 74HC165 input + 74HC595 output chain.
constexpr uint8_t ShiftDataIn = PIN_PC0;       // A0, 74HC165 QH
constexpr uint8_t ShiftOutputLatch = PIN_PC1;  // A1, 74HC595 RCLK
constexpr uint8_t ShiftClock = PIN_PC2;        // A2, shared clock
constexpr uint8_t ShiftDataOut = PIN_PC3;      // A3, 74HC595 SER
constexpr uint8_t I2cSda = PIN_PC4;            // A4 / SDA (no I2C device fitted)
constexpr uint8_t I2cScl = PIN_PC5;            // A5 / SCL (no I2C device fitted)
constexpr uint8_t ShiftInputLoad = PIN_PD4;    // D4, 74HC165 /PL
constexpr uint8_t ShiftMasterReset = PIN_PD5;  // D5, 74HC595 /SRCLR
constexpr uint8_t ShiftClockEnable = PIN_PD7;  // D7, 74HC165 /CE
constexpr uint8_t ShiftOutputEnable = PIN_PB0; // D8, 74HC595 /OE

constexpr uint8_t Buzzer = PIN_PB1;            // D9
// Optional flow-control input. HIGH means the ESP is timing-critical and the
// AVR must defer unsolicited UART frames. D10 is free because PakarController
// has no 1-Wire hardware fitted.
constexpr uint8_t EspBusy = PIN_PB2;            // D10 <- NodeMCU D3/GPIO0
// PuzzleBoard wiring deliberately reuses the labelled SDA/SCL header for the
// TM1637's two-wire (non-I2C) bus. Keep D11..D13 dedicated to in-system
// programming so the display cannot load the ISP clock/data lines.
constexpr uint8_t Tm1637Data = I2cSda;          // A4 / SDA -> DIO
constexpr uint8_t RcTransmit = PIN_PD3;         // D3 / INT1, 433 MHz transmitter
constexpr uint8_t Tm1637Clock = I2cScl;         // A5 / SCL -> CLK
constexpr uint8_t RcReceive = PIN_PD2;          // D2 / INT0, 433 MHz receiver

// D11..D13 remain the dedicated MOSI/MISO/SCK programming path. The
// addressable LED output owns D6 (PD6) for comparative AVR diagnostics only.
constexpr uint8_t IspMosi = PIN_PB3;
constexpr uint8_t IspMiso = PIN_PB4;
constexpr uint8_t IspClock = PIN_PB5;
constexpr uint8_t AddressableLed = PIN_PD6;     // D6, WS2811 / WS2812B data

// The first four active-low 74HC165 bits drive the menu. Inputs 4 and 5 are
// reserved system-sense positions. The final two positions are monitored
// inputs and must never be treated as keys.
constexpr uint8_t KeyPrevious = 0;
constexpr uint8_t KeyNext = 1;
constexpr uint8_t KeyDecrease = 2;
constexpr uint8_t KeyIncrease = 3;
constexpr uint8_t SystemSense1 = 4;
constexpr uint8_t SystemSense2 = 5;
constexpr uint8_t BluetoothLedSense = 6;
constexpr uint8_t DoorReedSense = 7;

// PWM is strapped to 0x41 so it cannot collide with the INA219 at 0x40.
constexpr uint8_t Ina219Address = 0x40;
constexpr uint8_t PwmAddress = 0x41;
constexpr float PwmFrequencyHz = 1000.0F;

} // namespace BoardPins
