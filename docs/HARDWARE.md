# Hardware and wiring

## ESP8266 to ATmega328P

| Function | NodeMCU | ESP GPIO | ATmega328P |
|---|---|---:|---|
| ISP SCK | D5 | 14 | D13 / PB5 |
| ISP MISO | D6 | 12 | D12 / PB4 |
| ISP MOSI | D7 | 13 | D11 / PB3 |
| AVR reset | D1 | 5 | RESET, direct active-low/open-drain |
| SoftwareSerial ESP TX | D0 | 16 | D2 / PD2 software RX |
| SoftwareSerial ESP RX | D4 | 2 | D3 / PD3 software TX through 5 V to 3.3 V translation |
| LED data | D2 | 4 | MT16703 DIN through a 74AHCT125-class 5 V buffer |
| reference | GND | - | GND and LED signal ground |

The ISP wiring and direct active-low reset path have physically read the
ATmega328P signature `1E 95 0F`.

### Required electrical protection

- AVR MISO and AVR SoftwareSerial TX are 5 V outputs. Do not connect either directly to
  an ESP8266 input. Use a proper level translator or a verified divider.
- ESP MOSI/SCK/TX are 3.3 V. They meet a typical 5 V ATmega328P input-high
  threshold at 16 MHz, but a level translator is preferred for margin.
- Keep RESET open-drain so the ESP only pulls it low and never drives 3.3 V
  against a 5 V pull-up.
- The dedicated SoftwareSerial link leaves both hardware UARTs free, so the
  NodeMCU CP210x remains usable for logs and flashing.
- GPIO2/D4 is a boot strap. Add a 10 kOhm pull-up to 3.3 V and ensure AVR D3
  is idle/high (or disconnected) whenever the ESP resets.
- Never connect the 24 V LED supply to either MCU. Use a buck supply for the
  controller and a common signal ground.

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
| D11 / PB3 | TM1637 DIO and ISP MOSI |
| D13 / PB5 | TM1637 CLK and ISP SCK |
| D2 / PD2 | SoftwareSerial RX from ESP D0/GPIO16 |
| D3 / PD3 | SoftwareSerial TX to level shifter then ESP D4/GPIO2 |
| ADC6 | ninth active-low/threshold button |

The TM1637 shares MOSI/SCK because that is the proven ControllerBoardMini
wiring. During ISP the AVR is held in reset; display activity is undefined but
must not affect flash correctness. The current physical ISP read/write already
succeeded with this board topology.

## UART connection gate

Connect the dedicated SoftwareSerial link only after installing the AVR-TX
level shifter. Then:

1. ESP D0/GPIO16 -> AVR D2/PD2.
2. AVR D3/PD3 -> 1 kOhm -> ESP D4/GPIO2, with 2 kOhm from ESP D4 to ground.
3. Add 10 kOhm from ESP D4 to 3.3 V so GPIO2 remains high during boot.
4. Keep the common ground already used for ISP.
5. Leave D5/D6/D7/D1 connected for recovery ISP.
6. Power up, then verify `HELLO`, CRC error counters, bidirectional ping, reset,
   and a physical key event before enabling automatic updates.
