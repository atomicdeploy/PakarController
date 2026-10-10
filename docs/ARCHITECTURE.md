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
authoritative state with bounded payloads and sequence identifiers. Two
compile-time-matched link policies are supported. `polling` requests AVR state
at 10 Hz and defers a 480-pixel LED write long enough to receive the complete
38400-baud response. `busy-wire` uses ESP D3/GPIO0 to AVR D10/PB2: HIGH blocks
AVR transmission during timing-sensitive LED output; LOW permits changed-state
pushes plus a one-second heartbeat. Physical key decisions remain on the AVR in
both variants.

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

### Snake example

Snake runs on the ESP's 10 by 8 logical cell model and therefore uses no AVR
framebuffer RAM. The first four physical inputs map to up/right/down/left and
input 9 restarts the game; Web, desktop, MQTT, and outbound-WebSocket commands
can drive the same directions. The AVR continues to own debouncing and shows
`SnAk` on TM1637. The LED renderer maps the game through the same serpentine,
six-pixels-per-cell geometry used by normal cell control.

## Programming and storage modes

The installed ESP image always contains every service. Runtime ownership is
temporarily exclusive only while ISP holds the AVR in reset:

1. normal UART protocol + LED/UI service;
2. AVR ISP backup/program/verify while AVR is held in reset;
3. later Urboot transparent UART programming immediately after ESP-controlled
   reset.

LittleFS stores a 32 KiB merged Urboot+application image, 1 KiB EEPROM image,
fuse/lock metadata, and a previous-good backup. Programming requires a valid
ATmega328P signature and a complete 32,768-byte raw image. It backs up first,
does not write fuses, and verifies every flash byte. Before ordinary updates,
the ESP plays the AVR update-start cue and arms a checksum-backed EEPROM
`Prog` latch. While latched the AVR keeps shift outputs off, ignores game
commands and keys, mutes ordinary boot audio, and holds `Prog` on TM1637. The
ESP clears the latch only after byte verification and recovered UART identity,
then plays the success cue. The initial migration from the pre-latch image is
the sole compatibility exception: it uses the legacy tone command after a full
backup, installs the latch-capable image, and proves arm/clear before reporting
success.

## Network surfaces

- responsive embedded web interface;
- REST status, mode, cell, game reset, AVR backup/program, and artifact routes;
- authenticated multi-client WebSocket status/control and binary LED streaming;
- SNTP-synchronized UTC state while the station link is associated;
- HTTP self-OTA for ESP firmware;
- bounded UDP LED-frame chunks for live Wi-Fi rendering;
- simultaneous AP+station operation derived from the useful PU850 network
  policy: the recovery AP remains available even when station association
  fails; checksum-backed LittleFS settings never block boot;
- mDNS identity and configurable SoftAP fallback;
- a single-client password-authenticated Telnet shell using the same validated
  command handlers as MQTT/WebSocket, advertised through mDNS;
- checksum-backed, disabled-by-default cloud settings with an authenticated
  TLS MQTT client and a separately configurable outbound WebSocket client;
- retained MQTT status/availability and inbound JSON commands on per-device
  topics, with device and controller credentials intended to use distinct ACLs;
- a simultaneous framed PC protocol on the NodeMCU USB UART. The NodeMCU uses
  SoftwareSerial only for the AVR link, and the AVR keeps UART0 in hardware.

The native Windows front panel can keep HTTP, WebSocket, and USB serial active
at the same time. It renders only device-advertised state, controls all current
hardware-compatible modes/peripherals, and streams generated or primary-screen
sampled LED frames without mpv.
State-changing network endpoints require authentication. First-boot
development credentials are temporary and must be replaced before installation.
