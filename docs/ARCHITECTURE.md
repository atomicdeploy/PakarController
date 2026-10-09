# Architecture

## State ownership

The AVR owns facts that require deterministic physical timing: debounced keys,
first-winner arbitration, shift-register outputs, TM1637 contents, buzzer
playback, and the current game mode. The ESP owns the LED framebuffer, cell
geometry, effects, Wi-Fi clients, APIs, stored images, and update operations.

The reusable 40LED `rainbow` and `fire` patterns are retained on the ESP.
Generic demo patterns that do not serve either game mode are deliberately
excluded. Selecting a direct cell or winner state returns to authoritative
logical-cell rendering; UDP streaming can replace the physical pixel frame
without consuming AVR SRAM.

Neither side invents the other's state. UART frames carry commands and
authoritative events with bounded payloads and sequence identifiers.

## Modes

### Cell selection

All 80 cells start white. Commands can set one cell to white, red, or off with
an effect. API clients can address a cell directly. The physical panel uses
previous/next and coarse navigation actions while TM1637 shows the selected
cell; mappings remain configurable because the final keypad legends are not
yet recorded.

The default geometry is 10 columns by 8 rows. Cell 1 is the rightmost bottom
cell; the chain travels upward, one column left, downward, and repeats.
`sectorsPerCell` is either 1 (80 addressable drivers) or 6 (480 drivers).

### First press wins

Inputs 1-8 come from the 74HC165 and input 9 comes from ADC6. The AVR chooses
the first debounced down-edge and latches it until reset. If multiple keys
settle in the same service pass, the lowest numbered key wins deterministically.
Outputs 1-8 use the 74HC595; winner 9 is always represented on TM1637, buzzer,
UART, and the ESP LED field even when only one eight-bit output register is
fitted. By default the 80 logical cells are divided into nine contiguous
winner regions; the winning region turns red and all others turn off. This
mapping is deliberately isolated in `LedGrid::showWinner` for adaptation to
the final physical strip layout.

## Programming and storage modes

ESP operation is mutually exclusive:

1. normal UART protocol + LED/UI service;
2. AVR ISP backup/program/verify while AVR is held in reset;
3. later Urboot transparent UART programming immediately after ESP-controlled
   reset.

LittleFS stores a 32 KiB merged Urboot+application image, 1 KiB EEPROM image,
fuse/lock metadata, and a previous-good backup. Programming requires a valid
ATmega328P signature and a complete 32,768-byte raw image. It backs up first,
does not write fuses, and verifies every flash byte.

## Network surfaces

- responsive embedded web interface;
- REST status, mode, cell, game reset, AVR backup/program, and artifact routes;
- HTTP self-OTA for ESP firmware;
- bounded UDP LED-frame chunks for live Wi-Fi rendering;
- mDNS identity and SoftAP fallback.
- one JSON health record per second on the NodeMCU hardware USB UART, leaving
  application traffic on the dedicated SoftwareSerial pins.

State-changing endpoints require authentication. First-boot development
credentials are temporary and must be replaced before installation.
