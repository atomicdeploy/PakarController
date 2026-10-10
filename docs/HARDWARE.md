# Hardware and wiring

## ESP8266 to ATmega328P

| Function | NodeMCU | ESP GPIO | ATmega328P |
|---|---|---:|---|
| ISP SCK | D5 | 14 | D13 / PB5 |
| ISP MISO | D6 | 12 | D12 / PB4 |
| ISP MOSI | D7 | 13 | D11 / PB3 |
| AVR reset | D1 | 5 | RESET, direct active-low/open-drain |
| SoftwareSerial ESP TX | D0 | 16 | D0 / PD0 hardware RXD |
| SoftwareSerial ESP RX | D4 | 2 | D1 / PD1 hardware TXD through 5 V to 3.3 V translation when required |
| LED data | D2 | 4 | MT16703/WS2811 DIN; direct 3.3 V logic is the verified test wiring |
| optional ESP busy | D3 | 0 | D10 / PB2; HIGH blocks AVR transmission, LOW permits it |
| reference | GND | - | GND and LED signal ground |

The ISP wiring and direct active-low reset path have physically read the
ATmega328P signature `1E 95 0F`.

### Required electrical protection

- AVR MISO and AVR TXD are 5 V outputs when the AVR is powered at 5 V. Do not connect either directly to
  an ESP8266 input. Use a proper level translator or a verified divider.
- ESP MOSI/SCK/TX are 3.3 V. They meet a typical 5 V ATmega328P input-high
  threshold at 16 MHz, but a level translator is preferred for margin.
- Keep RESET open-drain so the ESP only pulls it low and never drives 3.3 V
  against a 5 V pull-up.
- The ESP SoftwareSerial link leaves the NodeMCU CP210x usable for logs and
  flashing; the AVR uses UART0 for reliable non-blocking communication.
- GPIO2/D4 is a boot strap. Add a 10 kOhm pull-up to 3.3 V and ensure AVR TXD
  is idle/high (or disconnected) whenever the ESP resets.
- GPIO0/D3 is also a boot strap. The optional busy wire therefore uses a 10
  kOhm pull-up to 3.3 V. Its boot-high state is intentionally safe: it keeps
  the AVR quiet until the ESP application releases the line LOW.
- Never connect the 24 V LED supply to either MCU. Use a buck supply for the
  controller and a common signal ground.
- The current strip accepts the ESP's 3.3 V data directly. A short series
  resistor and 74AHCT125-class buffer remain installation-hardening options,
  not requirements for the verified bench setup.

## AVR peripheral ownership

| AVR pin | Purpose |
|---|---|
| A0 / PC0 | 74HC165 QH input |
| A1 / PC1 | 74HC595 latch |
| A2 / PC2 | shared shift clock |
| A3 / PC3 | 74HC595 serial data |
| D4 / PD4 | 74HC165 parallel load |
| D5 / PD5 | 74HC595 master reset |
| D7 / PD7 | 74HC165 clock enable |
| D8 / PB0 | 74HC595 output enable |
| D9 / PB1 | buzzer |
| A4 / PC4 / SDA | TM1637 DIO (two-wire TM bus, not I2C) |
| A5 / PC5 / SCL | TM1637 CLK (two-wire TM bus, not I2C) |
| D10 / PB2 | optional ESP-busy input; replaces the unused historical 1-Wire pin |
| D11 / PB3 | dedicated ISP MOSI |
| D12 / PB4 | dedicated ISP MISO |
| D13 / PB5 | dedicated ISP SCK |
| D0 / PD0 / RXD | Hardware UART RX from ESP D0/GPIO16 |
| D1 / PD1 / TXD | Hardware UART TX to level shifter then ESP D4/GPIO2 when required |
| ADC6 | ninth active-low/threshold button |

PuzzleBoard, rather than ControllerBoardMini, is authoritative for this
display connector: `SDA/A4 -> DIO` and `SCL/A5 -> CLK`. The labels describe the
header positions only; TM1637 is not I2C and no I2C module is enabled. The
independent live probe now receives a TM1637 ACK on these pins. Connect module
GND to the shared ground and power its VCC from the same 3.3 V test rail.

The ISP path remains isolated on D11-D13 and has completed an integrated
32-KiB write plus byte verification while the full PakarController ESP
application remained installed.

## Current bench wiring chart

| From | To | Purpose / state |
|---|---|---|
| NodeMCU D5 / GPIO14 | AVR D13 / PB5 | ISP SCK, verified |
| NodeMCU D6 / GPIO12 | AVR D12 / PB4 | ISP MISO, verified |
| NodeMCU D7 / GPIO13 | AVR D11 / PB3 | ISP MOSI, verified |
| NodeMCU D1 / GPIO5 | AVR RESET | active-low/open-drain reset, verified |
| NodeMCU D0 / GPIO16 | AVR D0 / PD0 / RXD | ESP SoftwareSerial TX to AVR hardware UART RX |
| AVR D1 / PD1 / TXD | NodeMCU D4 / GPIO2 | AVR hardware UART TX to ESP SoftwareSerial RX |
| NodeMCU D2 / GPIO4 | WS2811 DIN | full-brightness rainbow verified directly at 3.3 V |
| NodeMCU D3 / GPIO0 | AVR D10 / PB2 | optional busy wire; add 10 kOhm D3-to-3.3 V pull-up |
| AVR A4 / PC4 / SDA | TM1637 DIO | ACK verified |
| AVR A5 / PC5 / SCL | TM1637 CLK | ACK verified |
| AVR D9 / PB1 | buzzer signal | Timer1 melody output, audible |
| all controller grounds | common ground | required reference |

## UART connection gate

Connect the UART link after installing AVR-TX level translation if the AVR is
powered above 3.3 V. Then:

1. ESP D0/GPIO16 -> AVR D0/PD0/RXD.
2. AVR D1/PD1/TXD -> ESP D4/GPIO2 directly at 3.3 V, or through the documented
   divider/translator when the AVR is powered at 5 V.
3. Add 10 kOhm from ESP D4 to 3.3 V so GPIO2 remains high during boot.
4. Keep the common ground already used for ISP.
5. Leave D5/D6/D7/D1 connected for recovery ISP.
6. Power up, then verify `HELLO`, CRC error counters, bidirectional ping, reset,
   and a physical key event before enabling automatic updates.

The polling build ignores D3 and D10. The busy-wire build must be installed on
both controllers only after the wire and pull-up are fitted. In that build the
AVR pushes changed state plus a one-second heartbeat only while D3 is LOW; the
ESP raises D3 around timing-sensitive LED output.
